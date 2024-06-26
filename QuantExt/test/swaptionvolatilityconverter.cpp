/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "swaptionmarketdata.hpp"
#include "toplevelfixture.hpp"
#include "yieldcurvemarketdata.hpp"
#include <boost/test/unit_test.hpp>

#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>

#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/assign/list_of.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace boost::assign;

namespace {
// Variables to be used in the test
struct CommonVars {
    // Constructor
    CommonVars() : referenceDate(5, Feb, 2016) {
        // Reference date
        Settings::instance().evaluationDate() = referenceDate;

        // Link ibor index to correct forward curve
        QuantLib::ext::shared_ptr<IborIndex> iborIndex = conventions.floatIndex->clone(yieldCurves.forward6M);

        // Create underlying swap conventions
        swapConventions = QuantLib::ext::make_shared<SwapConventions>(conventions.settlementDays, conventions.fixedTenor,
                                                              conventions.fixedCalendar, conventions.fixedConvention,
                                                              conventions.fixedDayCounter, iborIndex);

        // Set up the various swaption matrices
        atmNormalVolMatrix = QuantLib::ext::make_shared<SwaptionVolatilityMatrix>(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, atmVols.nVols, Actual365Fixed(), true, Normal);

        atmLogNormalVolMatrix = QuantLib::ext::make_shared<SwaptionVolatilityMatrix>(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, atmVols.lnVols, Actual365Fixed(), true, ShiftedLognormal);

        // QuantLib::ext::make_shared can only handle 9 parameters
        atmShiftedLogNormalVolMatrix_1 = QuantLib::ext::shared_ptr<SwaptionVolatilityMatrix>(new SwaptionVolatilityMatrix(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, atmVols.slnVols_1, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_1));

        atmShiftedLogNormalVolMatrix_2 = QuantLib::ext::shared_ptr<SwaptionVolatilityMatrix>(new SwaptionVolatilityMatrix(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, atmVols.slnVols_2, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_2));
    }

    // Members
    Date referenceDate;
    SwaptionConventionsEUR conventions;
    SwaptionVolatilityEUR atmVols;
    YieldCurveEUR yieldCurves;
    QuantLib::ext::shared_ptr<SwapConventions> swapConventions;
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atmNormalVolMatrix;
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atmLogNormalVolMatrix;
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_1;
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_2;
    SavedSettings backup;
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(SwaptionVolatilityConverterTest)

BOOST_AUTO_TEST_CASE(testNormalToLognormal) {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from normal to lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Lognormal with no shifts)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmNormalVolMatrix, vars.yieldCurves.discountEonia,
                                          vars.yieldCurves.discountEonia, vars.swapConventions, vars.swapConventions,
                                          1 * Years, 30 * Years, ShiftedLognormal);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.lnVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLognormalToNormal) {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from lognormal to normal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Lognormal with no shifts -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmLogNormalVolMatrix,
                                          vars.yieldCurves.discountEonia, vars.yieldCurves.discountEonia,
                                          vars.swapConventions, vars.swapConventions, 1 * Years, 30 * Years, Normal);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testNormalToShiftedLognormal) {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from normal to shifted lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Shifted Lognormal with shift set 1)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmNormalVolMatrix, vars.yieldCurves.discountEonia,
                                          vars.yieldCurves.discountEonia, vars.swapConventions, vars.swapConventions,
                                          1 * Years, 30 * Years, ShiftedLognormal, vars.atmVols.shifts_1);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.slnVols_1[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToShiftedLognormal) {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from shifted lognormal to shifted lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Shifted Lognormal with shift set 1)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_1,
                                          vars.yieldCurves.discountEonia, vars.yieldCurves.discountEonia,
                                          vars.swapConventions, vars.swapConventions, 1 * Years, 30 * Years,
                                          ShiftedLognormal, vars.atmVols.shifts_2);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.slnVols_2[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToNormal) {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from shifted lognormal to normal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Shifted Lognormal with shift set 2 -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_2,
                                          vars.yieldCurves.discountEonia, vars.yieldCurves.discountEonia,
                                          vars.swapConventions, vars.swapConventions, 1 * Years, 30 * Years, Normal);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testFailureImplyingVol) {
    BOOST_TEST_MESSAGE("Testing failure to imply lognormal vol from normal vol...");

    CommonVars vars;

    // Normal volatility matrix where we cannot imply lognormal vol at 3M x 1Y point
    vector<Period> optionTenors = list_of(Period(3, Months))(Period(1, Years));
    vector<Period> swapTenors = list_of(Period(1, Years))(Period(5, Years));
    Matrix normalVols(2, 2);
    normalVols[0][0] = 0.003340;
    normalVols[0][1] = 0.004973;
    normalVols[1][0] = 0.003543;
    normalVols[1][1] = 0.005270;

    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> volMatrix = QuantLib::ext::make_shared<SwaptionVolatilityMatrix>(
        vars.referenceDate, vars.conventions.fixedCalendar, vars.conventions.fixedConvention, optionTenors, swapTenors,
        normalVols, Actual365Fixed(), true, Normal);

    // Set up the converter (Normal -> Lognormal)
    SwaptionVolatilityConverter converter(vars.referenceDate, volMatrix, vars.yieldCurves.discountEonia,
                                          vars.yieldCurves.discountEonia, vars.swapConventions, vars.swapConventions,
                                          1 * Years, 30 * Years, ShiftedLognormal);

    // We expect the conversion to fail
    BOOST_CHECK_THROW(converter.convert(), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNormalShiftsIgnored) {
    BOOST_TEST_MESSAGE("Testing shifts supplied to normal converter ignored...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Lognormal with no shifts -> Normal)
    // We supply target shifts but they are ignored since target type is Normal
    SwaptionVolatilityConverter converter(
        vars.referenceDate, vars.atmLogNormalVolMatrix, vars.yieldCurves.discountEonia, vars.yieldCurves.discountEonia,
        vars.swapConventions, vars.swapConventions, 1 * Years, 30 * Years, Normal, vars.atmVols.shifts_1);

    // Get back converted volatility structure and test result on pillar points
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testConstructionFromSwapIndex) {
    BOOST_TEST_MESSAGE("Testing construction of SwaptionVolatilityConverter from SwapIndex...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up a SwapIndex
    QuantLib::ext::shared_ptr<SwapIndex> swapIndex =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, vars.yieldCurves.forward6M, vars.yieldCurves.discountEonia);

    // Set up the converter using swap index (Shifted Lognormal with shift set 2 -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_2, swapIndex, swapIndex,
                                          Normal);

    // Test that the results are still ok
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testConstructionFromSwapIndexNoDiscount) {
    BOOST_TEST_MESSAGE("Testing construction from SwapIndex with no exogenous discount curve...");

    CommonVars vars;

    // Set up a SwapIndex
    QuantLib::ext::shared_ptr<SwapIndex> swapIndex =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, vars.yieldCurves.forward6M);

    // Set up the converter using swap index (Shifted Lognormal with shift set 2 -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_2, swapIndex, swapIndex,
                                          Normal);

    // Test that calling convert() still works
    BOOST_CHECK_NO_THROW(converter.convert());
}

BOOST_AUTO_TEST_CASE(testCube) {
    BOOST_TEST_MESSAGE("Testing lognormal to normal conversion for cube...");

    CommonVars vars;

    // Set up Swap Indices
    QuantLib::ext::shared_ptr<SwapIndex> shortSwapIndex =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(1 * Years, vars.yieldCurves.forward3M, vars.yieldCurves.discountEonia);
    QuantLib::ext::shared_ptr<SwapIndex> swapIndex =
        QuantLib::ext::make_shared<EuriborSwapIsdaFixA>(30 * Years, vars.yieldCurves.forward6M, vars.yieldCurves.discountEonia);

    // Set up a lognormal cube
    QuantLib::ext::shared_ptr<SwaptionVolatilityCube> cube = QuantLib::ext::make_shared<QuantExt::SwaptionVolCube2>(
        Handle<SwaptionVolatilityStructure>(vars.atmLogNormalVolMatrix), vars.atmVols.optionTenors,
        vars.atmVols.swapTenors, vars.atmVols.strikeSpreads, vars.atmVols.lnVolSpreads, swapIndex, shortSwapIndex,
        false, true);

    // Convert the cube to normal
    SwaptionVolatilityConverter converter(vars.referenceDate, cube, swapIndex, shortSwapIndex, Normal);
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();

    // Price swaptions in the lognormal and normal cube and compare their premiums
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            for (Size k = 0; k < vars.atmVols.strikeSpreads.size(); ++k) {
                Period optionTenor = vars.atmVols.optionTenors[i];
                Period swapTenor = vars.atmVols.swapTenors[j];
                Real atmStrike = cube->atmStrike(optionTenor, swapTenor);
                Real strikeSpread = vars.atmVols.strikeSpreads[k];
                Real strike = atmStrike + strikeSpread;
                if (strike > 0.0) {
                    Real inVol = cube->volatility(optionTenor, swapTenor, atmStrike + strikeSpread);
                    Real outVol = convertedsvs->volatility(optionTenor, swapTenor, atmStrike + strikeSpread);
                    Option::Type type = strikeSpread < 0.0 ? Option::Put : Option::Call;
                    Real tte = cube->optionTimes()[i];
                    Real inPrem = blackFormula(type, strike, atmStrike, inVol * std::sqrt(tte));
                    Real outPrem = bachelierBlackFormula(type, strike, atmStrike, outVol * std::sqrt(tte));
                    BOOST_CHECK_CLOSE(inPrem, outPrem, 0.01);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
