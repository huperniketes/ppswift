//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@_exported import Intents
import Foundation

#if os(iOS) || os(watchOS)
@available(iOS 11.0, watchOS 4.0, *)
extension INNotebookItemTypeResolutionResult {
    @nonobjc public
    static func disambiguation(with notebookItemTypesToDisambiguate: [INNotebookItemType]) -> Self {
        let numbers = notebookItemTypesToDisambiguate.map { NSNumber(value: $0.rawValue) }
        return __disambiguationWithNotebookItemTypes(toDisambiguate: numbers)
    }
}
#endif

