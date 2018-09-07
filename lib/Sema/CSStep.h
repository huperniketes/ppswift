//===--- CSFix.cpp - Constraint Fixes -------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file implements the \c SolverStep class and its related types,
// which is used by constraint solver to do iterative constraint solving.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SEMA_CSSTEP_H
#define SWIFT_SEMA_CSSTEP_H

#include "Constraint.h"
#include "ConstraintSystem.h"
#include "swift/AST/Types.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>

using namespace llvm;

namespace swift {
namespace constraints {

/// Represents a single independently solveable part of
/// the constraint system.
class SolverStep {
protected:
  ConstraintSystem &CS;

  /// Once step is complete this is a container to hold finalized solutions.
  SmallVectorImpl<Solution> &Solutions;

public:
  using StepResult =
      std::pair<ConstraintSystem::SolutionKind, SmallVector<SolverStep *, 4>>;

  explicit SolverStep(ConstraintSystem &cs,
                      SmallVectorImpl<Solution> &solutions)
      : CS(cs), Solutions(solutions) {}

  virtual ~SolverStep() {}

  /// Try to move solver forward by simplifying constraints if possible.
  /// Such simplication might lead to either producing a solution, or
  /// creating a set of "follow-up" more granular steps to execute.
  virtual StepResult advance() = 0;

protected:
  /// Erase constraint from the constraint system (include constraint graph)
  /// and return the constraint which follows it.
  ConstraintList::iterator erase(Constraint *constraint) {
    CS.CG.removeConstraint(constraint);
    return CS.InactiveConstraints.erase(constraint);
  }

  void restore(ConstraintList::iterator &iterator, Constraint *constraint) {
    CS.InactiveConstraints.insert(iterator, constraint);
    CS.CG.addConstraint(constraint);
  }

  ResolvedOverloadSetListItem *getResolvedOverloads() const {
    return CS.resolvedOverloadSets;
  }

  Score getCurrentScore() const { return CS.CurrentScore; }

  void filterSolutions(SmallVectorImpl<Solution> &solutions, bool minimize) {
    if (!CS.retainAllSolutions())
      CS.filterSolutions(solutions, CS.solverState->ExprWeights, minimize);
  }
};

class SplitterStep final : public SolverStep {
  enum class StepState {
    /// Split the system into independently solvable
    /// component steps.
    Split,
    /// Try to merge solutions produced by each component
    /// to form overall partial or final solution(s).
    Merge
  };

  StepState State = StepState::Split;

  // Partial solutions associated with given step, each element
  // of the array presents a disjoint component (or follow-up step)
  // that current step has been split into.
  unsigned NumComponents = 0;
  std::unique_ptr<SmallVector<Solution, 4>[]> PartialSolutions = nullptr;

  SmallVector<Constraint *, 4> OrphanedConstraints;

  SplitterStep(ConstraintSystem &cs, SmallVectorImpl<Solution> &solutions)
      : SolverStep(cs, solutions) {}

public:
  ~SplitterStep() override {
    auto &CG = CS.getConstraintGraph();
    CG.setOrphanedConstraints(std::move(OrphanedConstraints));
  }

  StepResult advance() override;

  static SplitterStep *create(ConstraintSystem &cs,
                              SmallVectorImpl<Solution> &solutions) {
    return new (cs.getAllocator()) SplitterStep(cs, solutions);
  }

private:
  /// If current step needs follow-up steps to get completely solved,
  /// let's compute them using connected components algorithm.
  void computeFollowupSteps(SmallVectorImpl<SolverStep *> &steps);

  /// Once all of the follow-up steps are complete, let's try
  /// to merge resulting solutions together, to form final solution(s)
  /// for this step.
  ///
  /// \returns true if there are any solutions, false otherwise.
  bool mergePartialSolutions() const;
};

class ComponentStep final : public SolverStep {
  class ComponentScope;

  unsigned Index;
  Score OriginalScore;

  // If this step depends on other smaller steps to be solved first
  // we need to keep active scope until all of the work is done.
  ComponentScope *Scope;

  // Type variables and constraints "in scope" of this step.
  SmallVector<TypeVariableType *, 16> TypeVars;
  SmallVector<Constraint *, 16> Constraints;

  /// Constraint which doesn't have any free type variables associated
  /// with it, which makes it disconnected in the graph.
  Constraint *OrphanedConstraint;

  ComponentStep(ConstraintSystem &cs, unsigned index,
                SmallVectorImpl<Solution> &solutions)
      : SolverStep(cs, solutions), Index(index),
        OriginalScore(getCurrentScore()) {}

public:
  ~ComponentStep() {
    if (!Scope)
      return;

    // Since the step is no longer needed, it's same to rewind active scope.
    delete Scope;
    Scope = nullptr;
  }

  /// Record a type variable as associated with this step.
  void record(TypeVariableType *typeVar) { TypeVars.push_back(typeVar); }

  /// Record a constraint as associated with this step.
  void record(Constraint *constraint) { Constraints.push_back(constraint); }

  /// Record a constraint as associated with this step but which doesn't
  /// have any free type variables associated with it.
  void recordOrphan(Constraint *constraint) {
    assert(!OrphanedConstraint);
    OrphanedConstraint = constraint;
  }

  StepResult advance() override;

  static ComponentStep *create(ConstraintSystem &cs, unsigned index,
                               SmallVectorImpl<Solution> &solutions) {
    return new (cs.getAllocator()) ComponentStep(cs, index, solutions);
  }

private:
  class ComponentScope {
    friend class SolverStep;

    ConstraintSystem &CS;
    ConstraintSystem::SolverScope *SolverScope;

    SmallVector<TypeVariableType *, 16> TypeVars;
    ConstraintList Constraints;

    ConstraintSystem::SolverScope *PrevPartialScope = nullptr;

  public:
    ComponentScope(ComponentStep &component);

    ~ComponentScope() {
      delete SolverScope; // rewind back all of the changes.
      CS.solverState->PartialSolutionScope = PrevPartialScope;

      // return all of the saved type variables back to the system.
      CS.TypeVariables = std::move(TypeVars);
      // return all of the saved constraints back to the system.
      CS.InactiveConstraints.splice(CS.InactiveConstraints.end(), Constraints);
    }
  };
};

class TypeVariableStep final : public SolverStep {
  using BindingContainer = ConstraintSystem::PotentialBindings;
  using Binding = ConstraintSystem::PotentialBinding;

  TypeVariableType *TypeVar;
  SmallVector<Binding, 4> Bindings;

  TypeVariableStep(ConstraintSystem &cs, BindingContainer &bindings,
                   SmallVectorImpl<Solution> &solutions)
      : SolverStep(cs, solutions), TypeVar(bindings.TypeVar),
        Bindings(bindings.Bindings.begin(), bindings.Bindings.end()) {}

public:
  StepResult advance() override;

  static TypeVariableStep *create(ConstraintSystem &cs,
                                  BindingContainer &bindings,
                                  SmallVectorImpl<Solution> &solutions) {
    return new (cs.getAllocator()) TypeVariableStep(cs, bindings, solutions);
  }
};

class DisjunctionStep final : public SolverStep {
  Constraint *Disjunction;
  SmallVector<Constraint *, 4> DisabledChoices;
  ConstraintList::iterator AfterDisjunction;

  DisjunctionChoiceProducer Producer;

  Optional<Score> BestNonGenericScore;
  Optional<std::pair<DisjunctionChoice, Score>> LastSolvedChoice;

  /// Scope initialized when attempting each disjunction choice.
  ConstraintSystem::SolverScope *ActiveScope = nullptr;

public:
  DisjunctionStep(ConstraintSystem &cs, Constraint *disjunction,
                  SmallVectorImpl<Solution> &solutions)
      : SolverStep(cs, solutions), Disjunction(disjunction),
        AfterDisjunction(erase(disjunction)), Producer({cs, disjunction}) {
    assert(Disjunction->getKind() == ConstraintKind::Disjunction);
    pruneOverloadSet(Disjunction);
    ++cs.solverState->NumDisjunctions;
  }

  ~DisjunctionStep() override {
    restore(AfterDisjunction, Disjunction);
    // Re-enable previously disabled overload choices.
    for (auto *choice : DisabledChoices)
      choice->setEnabled();
  }

  StepResult advance() override;

  static DisjunctionStep *create(ConstraintSystem &cs, Constraint *disjunction,
                                 SmallVectorImpl<Solution> &solutions) {
    return new (cs.getAllocator()) DisjunctionStep(cs, disjunction, solutions);
  }

private:
  bool shouldSkipChoice(DisjunctionChoice &choice) const;

  // Check if selected disjunction has a representative
  // this might happen when there are multiple binary operators
  // chained together. If so, disable choices which differ
  // from currently selected representative.
  void pruneOverloadSet(Constraint *disjunction) {
    auto *choice = disjunction->getNestedConstraints().front();
    auto *typeVar = choice->getFirstType()->getAs<TypeVariableType>();
    if (!typeVar)
      return;

    auto *repr = typeVar->getImpl().getRepresentative(nullptr);
    if (!repr || repr == typeVar)
      return;

    for (auto *resolved = getResolvedOverloads(); resolved;
         resolved = resolved->Previous) {
      if (!resolved->BoundType->isEqual(repr))
        continue;

      auto &representative = resolved->Choice;
      if (!representative.isDecl())
        return;

      // Disable all of the overload choices which are different from
      // the one which is currently picked for representative.
      for (auto *constraint : disjunction->getNestedConstraints()) {
        auto choice = constraint->getOverloadChoice();
        if (!choice.isDecl() || choice.getDecl() == representative.getDecl())
          continue;

        constraint->setDisabled();
        DisabledChoices.push_back(constraint);
      }
      break;
    }
  };
};

} // end namespace constraints
} // end namespace swift

#endif // SWIFT_SEMA_CSSTEP_H