// RUN: %target-run-simple-swift
// REQUIRES: executable_test

// REQUIRES: objc_interop
// UNSUPPORTED: OS=watchos

import StdlibUnittest
import Accelerate

var Accelerate_vDSPIntegrationTests = TestSuite("Accelerate_vDSPIntegration")

//===----------------------------------------------------------------------===//
//
//  vDSP integration tests
//
//===----------------------------------------------------------------------===//

if #available(iOS 9999, macOS 9999, tvOS 9999, watchOS 9999, *) {
    
    let count = 1024
    let n = vDSP_Length(1024)
    
    let sourcef: [Float] = (0 ..< 1024).map {
        return sin(Float($0) * 0.03) * cos(Float($0) * 0.07)
    }
    
    let sourced: [Double] = (0 ..< 1024).map {
        return sin(Double($0) * 0.03) * cos(Double($0) * 0.07)
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/SinglePrecisionRunningSum") {
        var result = [Float](repeating: 0,
                             count: count)
        
        vDSP.integrate(sourcef,
                       using: .runningSum,
                       result: &result)
        
        var legacyResult = [Float](repeating: -1,
                                   count: count)
        
        vDSP_vrsum(sourcef, 1,
                   [1],
                   &legacyResult, 1,
                   n)
        
        let returnedResult = vDSP.integrate(sourcef,
                                            using: .runningSum)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/SinglePrecisionTrapezoidal") {
        var result = [Float](repeating: 0,
                             count: count)
        
        vDSP.integrate(sourcef,
                       using: .simpson,
                       result: &result)
        
        var legacyResult = [Float](repeating: -1,
                                   count: count)
        
        vDSP_vsimps(sourcef, 1,
                    [1],
                    &legacyResult, 1,
                    n)
        
        let returnedResult = vDSP.integrate(sourcef,
                                            using: .simpson)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/SinglePrecisionTrapezoidal") {
        var result = [Float](repeating: 0,
                             count: count)
        
        vDSP.integrate(sourcef,
                       using: .trapezoidal,
                       result: &result)
        
        var legacyResult = [Float](repeating: -1,
                                   count: count)
        
        vDSP_vtrapz(sourcef, 1,
                    [1],
                    &legacyResult, 1,
                    n)
        
        let returnedResult = vDSP.integrate(sourcef,
                                            using: .trapezoidal)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/DoublePrecisionRunningSum") {
        var result = [Double](repeating: 0,
                              count: count)
        
        vDSP.integrate(sourced,
                       using: .runningSum,
                       result: &result)
        
        var legacyResult = [Double](repeating: -1,
                                    count: count)
        
        vDSP_vrsumD(sourced, 1,
                    [1],
                    &legacyResult, 1,
                    n)
        
        let returnedResult = vDSP.integrate(sourced,
                                            using: .runningSum)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/DoublePrecisionSimpson") {
        var result = [Double](repeating: 0,
                              count: count)
        
        vDSP.integrate(sourced,
                       using: .simpson,
                       result: &result)
        
        var legacyResult = [Double](repeating: -1,
                                    count: count)
        
        vDSP_vsimpsD(sourced, 1,
                     [1],
                     &legacyResult, 1,
                     n)
        
        let returnedResult = vDSP.integrate(sourced,
                                            using: .simpson)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
    
    Accelerate_vDSPIntegrationTests.test("vDSP/DoublePrecisionTrapezoidal") {
        var result = [Double](repeating: 0,
                              count: count)
        
        vDSP.integrate(sourced,
                       using: .trapezoidal,
                       result: &result)
        
        var legacyResult = [Double](repeating: -1,
                                    count: count)
        
        vDSP_vtrapzD(sourced, 1,
                     [1],
                     &legacyResult, 1,
                     n)
        
        let returnedResult = vDSP.integrate(sourced,
                                            using: .trapezoidal)
        
        expectTrue(result.elementsEqual(legacyResult))
        expectTrue(result.elementsEqual(returnedResult))
    }
}

runAllTests()
