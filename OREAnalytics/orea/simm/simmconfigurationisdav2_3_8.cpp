/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_3_8.hpp>
#include <orea/simm/simmconfigurationisdav2_3_8.hpp>
#include <ql/math/matrix.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>

using QuantLib::InterestRateIndex;
using QuantLib::Matrix;
using QuantLib::Real;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

QuantLib::Size SimmConfiguration_ISDA_V2_3_8::group(const string& qualifier, const std::map<QuantLib::Size,
                                                    std::set<string>>& categories) const {
    QuantLib::Size result = 0;
    for (const auto& kv : categories) {
        if (kv.second.empty()) {
            result = kv.first;
        } else {
            if (kv.second.count(qualifier) > 0) {
                // Found qualifier in category so return it
                return kv.first;
            }
        }
    }

    // If we get here, result should hold category with empty set
    return result;
}

QuantLib::Real SimmConfiguration_ISDA_V2_3_8::weight(const RiskType& rt, boost::optional<string> qualifier,
                                                     boost::optional<std::string> label_1,
                                                     const std::string& calculationCurrency) const {

    if (rt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided weight");
        QL_REQUIRE(qualifier, "need a qualifier to return a risk weight for the risk type FX");

        QuantLib::Size g1 = group(calculationCurrency, ccyGroups_);
        QuantLib::Size g2 = group(*qualifier, ccyGroups_);
        return rwFX_[g1][g2];
    }

    return SimmConfigurationBase::weight(rt, qualifier, label_1);
}

QuantLib::Real SimmConfiguration_ISDA_V2_3_8::correlation(const RiskType& firstRt, const string& firstQualifier,
                                                          const string& firstLabel_1, const string& firstLabel_2,
                                                          const RiskType& secondRt, const string& secondQualifier,
                                                          const string& secondLabel_1, const string& secondLabel_2,
                                                          const std::string& calculationCurrency) const {

    if (firstRt == RiskType::FX && secondRt == RiskType::FX) {
        QL_REQUIRE(calculationCurrency != "", "no calculation currency provided corr");
        QuantLib::Size g = group(calculationCurrency, ccyGroups_);
        QuantLib::Size g1 = group(firstQualifier, ccyGroups_);
        QuantLib::Size g2 = group(secondQualifier, ccyGroups_);
        if (g == 0) {
            return fxRegVolCorrelation_[g1][g2];
        } else if (g == 1) {
            return fxHighVolCorrelation_[g1][g2];
        } else {
            QL_FAIL("FX Volatility group " << g << " not recognized");
        }
    }

    return SimmConfigurationBase::correlation(firstRt, firstQualifier, firstLabel_1, firstLabel_2, secondRt,
                                              secondQualifier, secondLabel_1, secondLabel_2);
}

SimmConfiguration_ISDA_V2_3_8::SimmConfiguration_ISDA_V2_3_8(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                             const QuantLib::Size& mporDays, const std::string& name,
                                                             const std::string version)
    : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {

    // The differences in methodology for 1 Day horizon is described in
    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:
    // Section I - Calibration with one-day horizon
    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, "SIMM only supports MPOR 10-day or 1-day");

    // Set up the correct concentration threshold getter
    if (mporDays == 10) {
        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_V2_3_8>(simmBucketMapper_);
    } else {
        // SIMM:Technical Paper, Section I.4: "The Concentration Risk feature is disabled"
        simmConcentration_ = boost::make_shared<SimmConcentrationBase>();
    }

    // clang-format off

    // Set up the members for this configuration
    // Explanations of all these members are given in the hpp file
    
    mapBuckets_ = { 
        { RiskType::IRCurve, { "1", "2", "3" } },
        { RiskType::CreditQ, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::CreditNonQ, { "1", "2", "Residual" } },
        { RiskType::CreditVolNonQ, { "1", "2", "Residual" } },
        { RiskType::Equity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::EquityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual" } },
        { RiskType::Commodity, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } },
        { RiskType::CommodityVol, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17" } }
    };

    mapLabels_1_ = {
        { RiskType::IRCurve, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CreditQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::CreditNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::IRVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::InflationVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CreditVol, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::CreditVolNonQ, { "1y", "2y", "3y", "5y", "10y" } },
        { RiskType::EquityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::CommodityVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } },
        { RiskType::FXVol, { "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y" } }
    };

    mapLabels_2_ = {
        { RiskType::IRCurve, { "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime", "Municipal" } },
        { RiskType::CreditQ, { "", "Sec" } }
    };

    // Populate CCY groups that are used for FX correlations and risk weights
    // The groups consists of High Vol Currencies (ARS, BRL, MXN, TRY, ZAR) & regular vol currencies (all others)
    ccyGroups_ = {
        { 1, { "ARS", "BRL", "MXN", "TRY", "ZAR" } },
        { 0, { } },
    };

    vector<Real> temp;

    if (mporDays_ == 10) {
        // Risk weights
        temp = {
            7.3,  13.0, 
            13.0, 10.2
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 64 },
            { RiskType::XCcyBasis, 21 },
            { RiskType::IRVol, 0.18 },
            { RiskType::InflationVol, 0.18 },   
            { RiskType::CreditVol, 0.73 },
            { RiskType::CreditVolNonQ, 0.73 },
            { RiskType::CommodityVol, 0.61 },
            { RiskType::FXVol, 0.47 },
            { RiskType::BaseCorr, 11.0 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 81.0, 96.0, 86.0, 53.0, 59.0, 47.0, 181.0, 452.0, 252.0, 261.0, 218.0, 195.0, 452.0 } },
            { RiskType::CreditNonQ, { 280.0, 1200.0, 1200.0 } },
            { RiskType::Equity, { 25.0, 28.0, 30.0, 28.0, 23.0, 24.0, 29.0, 27.0, 31.0, 33.0, 19.0, 19.0, 33.0 } },
            { RiskType::Commodity, { 22.0, 29.0, 33.0, 25.0, 35.0, 24.0, 22.0, 49.0, 24.0, 53.0, 20.0, 21.0, 13.0, 15.0, 13.0, 53.0, 17.0 } },
            { RiskType::EquityVol, { 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.98, 0.50 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 114.0, 106.0, 95.0, 74.0, 66.0, 61.0, 56.0, 52.0, 53.0, 57.0, 60.0, 66.0 } },
            { { RiskType::IRCurve, "2" }, { 15.0, 18.0, 8.6, 11.0, 13.0, 15.0, 18.0, 20.0, 19.0, 19.0, 20.0, 23.0 } },
            { { RiskType::IRCurve, "3" }, { 101.0, 91.0, 78.0, 80.0, 90.0, 89.0, 94.0, 94.0, 92.0, 101.0, 104.0, 102.0 } }
        };
        
        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.54;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.64;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.55;
        hvr_ir_ = 0.44;
        // Curvature weights 
        curvatureWeights_ = {
            { RiskType::IRVol, { 0.5, 
                                 0.5 * 14.0 / (365.0 / 12.0), 
                                 0.5 * 14.0 / (3.0 * 365.0 / 12.0), 
                                 0.5 * 14.0 / (6.0 * 365.0 / 12.0), 
                                 0.5 * 14.0 / 365.0, 
                                 0.5 * 14.0 / (2.0 * 365.0), 
                                 0.5 * 14.0 / (3.0 * 365.0), 
                                 0.5 * 14.0 / (5.0 * 365.0), 
                                 0.5 * 14.0 / (10.0 * 365.0), 
                                 0.5 * 14.0 / (15.0 * 365.0), 
                                 0.5 * 14.0 / (20.0 * 365.0), 
                                 0.5 * 14.0 / (30.0 * 365.0) } 
            },
            { RiskType::CreditVol, { 0.5 * 14.0 / 365.0, 
                                     0.5 * 14.0 / (2.0 * 365.0), 
                                     0.5 * 14.0 / (3.0 * 365.0), 
                                     0.5 * 14.0 / (5.0 * 365.0), 
                                     0.5 * 14.0 / (10.0 * 365.0) } 
            }
        };
        curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];

    } else {
        // SIMM:Technical Paper, Section I.1: "All delta and vega risk weights should be replaced with the values for 
        // one-day calibration given in the Calibration Results document."
        
        // Risk weights
        temp = {
            1.8,  3.0, 
            3.0,  2.9
        };
        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());

        rwRiskType_ = {
            { RiskType::Inflation, 15 },
            { RiskType::XCcyBasis, 5.6 },
            { RiskType::IRVol, 0.046 },
            { RiskType::InflationVol, 0.046 },
            { RiskType::CreditVol, 0.1 },
            { RiskType::CreditVolNonQ, 0.1 },
            { RiskType::CommodityVol, 0.13 },
            { RiskType::FXVol, 0.099 },
            { RiskType::BaseCorr, 2.7 }
        };

        rwBucket_ = {
            { RiskType::CreditQ, { 23.0, 27.0, 18.0, 12.0, 13.0, 12.0, 49.0, 92.0, 48.0, 59.0, 41.0, 41.0, 92.0 } },
            { RiskType::CreditNonQ, { 74.0, 240.0, 240.0 } },
            { RiskType::Equity, { 8.3, 9.1, 9.8, 9.0, 7.7, 8.4, 9.3, 9.4, 9.9, 11.0, 6.0, 6.0, 11.0 } },
            { RiskType::Commodity, { 6.3, 9.1, 8.1, 7.2, 10.0, 8.0, 7.1, 11.0, 8.1, 16.0, 6.2, 6.2, 4.7, 4.8, 3.8, 16.0, 5.1 } },
            { RiskType::EquityVol, { 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.094, 0.27, 0.094 } }, 
        };
        
        rwLabel_1_ = {
            { { RiskType::IRCurve, "1" }, { 18.0, 15.0, 12.0, 12.0, 13.0, 15.0, 16.0, 16.0, 16.0, 17.0, 16.0, 17.0 } },
            { { RiskType::IRCurve, "2" }, { 1.7, 3.4, 1.6, 2.0, 3.0, 4.8, 5.8, 6.8, 6.5, 7.0, 7.5, 8.3 } },
            { { RiskType::IRCurve, "3" }, { 36.0, 27.0, 16.0, 19.0, 23.0, 23.0, 32.0, 31.0, 32.0, 30.0, 32.0, 27.0 } }
        };

        // Historical volatility ratios 
        historicalVolatilityRatios_[RiskType::EquityVol] = 0.5;
        historicalVolatilityRatios_[RiskType::CommodityVol] = 0.67;
        historicalVolatilityRatios_[RiskType::FXVol] = 0.73;
        hvr_ir_ = 0.5;

        // Curvature weights
        //SIMM:Technical Paper, Section I.3, this 10-day formula for curvature weights is modified
        curvatureWeights_ = {
            { RiskType::IRVol, { 0.5 / 10.0, 
                                 0.5 * 1.40 / (365.0 / 12.0), 
                                 0.5 * 1.40 / (3.0 * 365.0 / 12.0), 
                                 0.5 * 1.40 / (6.0 * 365.0 / 12.0), 
                                 0.5 * 1.40 / 365.0, 
                                 0.5 * 1.40 / (2.0 * 365.0), 
                                 0.5 * 1.40 / (3.0 * 365.0), 
                                 0.5 * 1.40 / (5.0 * 365.0), 
                                 0.5 * 1.40 / (10.0 * 365.0), 
                                 0.5 * 1.40 / (15.0 * 365.0), 
                                 0.5 * 1.40 / (20.0 * 365.0), 
                                 0.5 * 1.40 / (30.0 * 365.0) } 
            },
            { RiskType::CreditVol, { 0.5 * 1.40 / 365.0, 
                                     0.5 * 1.40 / (2.0 * 365.0), 
                                     0.5 * 1.40 / (3.0 * 365.0), 
                                     0.5 * 1.40 / (5.0 * 365.0), 
                                     0.5 * 1.40 / (10.0 * 365.0) } 
            }
        };
        curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];
        curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];
        
    }
    

    // Valid risk types
    validRiskTypes_ = {
        RiskType::Commodity,
        RiskType::CommodityVol,
        RiskType::CreditNonQ,
        RiskType::CreditQ,
        RiskType::CreditVol,
        RiskType::CreditVolNonQ,
        RiskType::Equity,
        RiskType::EquityVol,
        RiskType::FX,
        RiskType::FXVol,
        RiskType::Inflation,
        RiskType::IRCurve,
        RiskType::IRVol,
        RiskType::InflationVol,
        RiskType::BaseCorr,
        RiskType::XCcyBasis,
        RiskType::ProductClassMultiplier,
        RiskType::AddOnNotionalFactor,
        RiskType::PV,
        RiskType::Notional,
        RiskType::AddOnFixedAmount
    };

    // Risk class correlation matrix  
    temp = {
        1.00, 0.32, 0.19, 0.33, 0.41, 0.28,
        0.32, 1.00, 0.45, 0.69, 0.52, 0.42,
        0.19, 0.45, 1.00, 0.48, 0.40, 0.14,
        0.33, 0.69, 0.48, 1.00, 0.52, 0.34,
        0.41, 0.52, 0.40, 0.52, 1.00, 0.38,
        0.28, 0.42, 0.14, 0.34, 0.38, 1.00
    };
    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());

    // FX correlations
    temp = {
        0.500, 0.280,
        0.280, 0.690
    };
    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    temp = {
        0.850, 0.390,
        0.390, 0.500
    };
    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());

    // Interest rate tenor correlations (i.e. Label1 level correlations) 
    temp = {
        1.00, 0.75, 0.63, 0.55, 0.44, 0.35, 0.31, 0.26, 0.21, 0.17, 0.15, 0.14,
        0.75, 1.00, 0.79, 0.68, 0.51, 0.40, 0.33, 0.28, 0.22, 0.17, 0.15, 0.15,
        0.63, 0.79, 1.00, 0.85, 0.67, 0.53, 0.45, 0.38, 0.31, 0.23, 0.21, 0.22,
        0.55, 0.68, 0.85, 1.00, 0.82, 0.70, 0.61, 0.53, 0.44, 0.36, 0.35, 0.33,
        0.44, 0.51, 0.67, 0.82, 1.00, 0.94, 0.86, 0.78, 0.66, 0.60, 0.58, 0.56,
        0.35, 0.40, 0.53, 0.70, 0.94, 1.00, 0.96, 0.90, 0.80, 0.75, 0.72, 0.71,
        0.31, 0.33, 0.45, 0.61, 0.86, 0.96, 1.00, 0.97, 0.88, 0.83, 0.80, 0.78,
        0.26, 0.28, 0.38, 0.53, 0.78, 0.90, 0.97, 1.00, 0.95, 0.91, 0.88, 0.87,
        0.21, 0.22, 0.31, 0.44, 0.66, 0.80, 0.88, 0.95, 1.00, 0.97, 0.95, 0.95,
        0.17, 0.17, 0.23, 0.36, 0.60, 0.75, 0.83, 0.91, 0.97, 1.00, 0.98, 0.98,
        0.15, 0.15, 0.21, 0.35, 0.58, 0.72, 0.80, 0.88, 0.95, 0.98, 1.00, 0.99,
        0.14, 0.15, 0.22, 0.33, 0.56, 0.71, 0.78, 0.87, 0.95, 0.98, 0.99, 1.00
    };
    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());

    // CreditQ inter-bucket correlations 
    temp = {
        1.00, 0.35, 0.37, 0.35, 0.37, 0.34, 0.38, 0.31, 0.34, 0.33, 0.30, 0.31,
        0.35, 1.00, 0.44, 0.43, 0.45, 0.42, 0.32, 0.34, 0.38, 0.38, 0.35, 0.35,
        0.37, 0.44, 1.00, 0.48, 0.49, 0.47, 0.34, 0.35, 0.42, 0.42, 0.40, 0.39,
        0.35, 0.43, 0.48, 1.00, 0.48, 0.48, 0.32, 0.34, 0.40, 0.41, 0.39, 0.37,
        0.37, 0.45, 0.49, 0.48, 1.00, 0.48, 0.34, 0.35, 0.41, 0.41, 0.40, 0.39,
        0.34, 0.42, 0.47, 0.48, 0.48, 1.00, 0.31, 0.33, 0.37, 0.38, 0.38, 0.36,
        0.38, 0.32, 0.34, 0.32, 0.34, 0.31, 1.00, 0.28, 0.32, 0.30, 0.27, 0.28,
        0.31, 0.34, 0.35, 0.34, 0.35, 0.33, 0.28, 1.00, 0.32, 0.32, 0.29, 0.29,
        0.34, 0.38, 0.42, 0.40, 0.41, 0.37, 0.32, 0.32, 1.00, 0.38, 0.35, 0.35,
        0.33, 0.38, 0.42, 0.41, 0.41, 0.38, 0.30, 0.32, 0.38, 1.00, 0.35, 0.34,
        0.30, 0.35, 0.40, 0.39, 0.40, 0.38, 0.27, 0.29, 0.35, 0.35, 1.00, 0.33,
        0.31, 0.35, 0.39, 0.37, 0.39, 0.36, 0.28, 0.29, 0.35, 0.34, 0.33, 1.00
    };
    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());

    // Equity inter-bucket correlations
    temp = {
        1.00, 0.20, 0.21, 0.21, 0.15, 0.19, 0.19, 0.19, 0.18, 0.14, 0.24, 0.24,
        0.20, 1.00, 0.25, 0.24, 0.16, 0.21, 0.22, 0.21, 0.21, 0.16, 0.27, 0.27,
        0.21, 0.25, 1.00, 0.26, 0.17, 0.22, 0.24, 0.22, 0.23, 0.17, 0.28, 0.28,
        0.21, 0.24, 0.26, 1.00, 0.18, 0.24, 0.25, 0.25, 0.23, 0.19, 0.31, 0.31,
        0.15, 0.16, 0.17, 0.18, 1.00, 0.27, 0.27, 0.27, 0.15, 0.20, 0.32, 0.32,
        0.19, 0.21, 0.22, 0.24, 0.27, 1.00, 0.36, 0.35, 0.20, 0.25, 0.42, 0.42,
        0.19, 0.22, 0.24, 0.25, 0.27, 0.36, 1.00, 0.34, 0.20, 0.26, 0.43, 0.43,
        0.19, 0.21, 0.22, 0.25, 0.27, 0.35, 0.34, 1.00, 0.20, 0.25, 0.41, 0.41,
        0.18, 0.21, 0.23, 0.23, 0.15, 0.20, 0.20, 0.20, 1.00, 0.16, 0.26, 0.26,
        0.14, 0.16, 0.17, 0.19, 0.20, 0.25, 0.26, 0.25, 0.16, 1.00, 0.29, 0.29,
        0.24, 0.27, 0.28, 0.31, 0.32, 0.42, 0.43, 0.41, 0.26, 0.29, 1.00, 0.54,
        0.24, 0.27, 0.28, 0.31, 0.32, 0.42, 0.43, 0.41, 0.26, 0.29, 0.54, 1.00
    };
    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());

    // Commodity inter-bucket correlations
    temp = {
        1.00, 0.36, 0.23, 0.30, 0.30, 0.07, 0.32, 0.02, 0.26, 0.20, 0.17, 0.15, 0.21, 0.15, 0.19, 0.00, 0.24,
        0.36, 1.00, 0.93, 0.94, 0.88, 0.16, 0.21, 0.09, 0.21, 0.20, 0.40, 0.30, 0.24, 0.29, 0.23, 0.00, 0.56,
        0.23, 0.93, 1.00, 0.91, 0.85, 0.06, 0.21, 0.04, 0.21, 0.19, 0.33, 0.23, 0.14, 0.23, 0.25, 0.00, 0.50,
        0.30, 0.94, 0.91, 1.00, 0.83, 0.06, 0.24, 0.04, 0.21, 0.17, 0.36, 0.25, 0.14, 0.25, 0.20, 0.00, 0.53,
        0.30, 0.88, 0.85, 0.83, 1.00, 0.10, 0.17, 0.04, 0.16, 0.17, 0.40, 0.33, 0.25, 0.30, 0.19, 0.00, 0.53,
        0.07, 0.16, 0.06, 0.06, 0.10, 1.00, 0.27, 0.50, 0.20, 0.04, 0.17, 0.08, 0.12, 0.08, 0.14, 0.00, 0.25,
        0.32, 0.21, 0.21, 0.24, 0.17, 0.27, 1.00, 0.27, 0.61, 0.18, 0.06,-0.11, 0.12, 0.08, 0.08, 0.00, 0.22,
        0.02, 0.09, 0.04, 0.04, 0.04, 0.50, 0.27, 1.00, 0.19, 0.00, 0.12,-0.03, 0.09, 0.05, 0.07, 0.00, 0.14,
        0.26, 0.21, 0.21, 0.21, 0.16, 0.20, 0.61, 0.19, 1.00, 0.14, 0.13,-0.07, 0.07, 0.06, 0.12, 0.00, 0.19,
        0.20, 0.20, 0.19, 0.17, 0.17, 0.04, 0.18, 0.00, 0.40, 1.00, 0.11, 0.13, 0.07, 0.06, 0.06, 0.00, 0.11,
        0.17, 0.40, 0.33, 0.36, 0.40, 0.17, 0.06, 0.12, 0.13, 0.11, 1.00, 0.31, 0.27, 0.21, 0.20, 0.00, 0.37,
        0.15, 0.30, 0.23, 0.25, 0.33, 0.08,-0.11,-0.03,-0.07, 0.13, 0.31, 1.00, 0.15, 0.19, 0.10, 0.00, 0.23,
        0.21, 0.24, 0.14, 0.14, 0.25, 0.12, 0.12, 0.09, 0.07, 0.07, 0.27, 0.15, 1.00, 0.28, 0.20, 0.00, 0.27,
        0.15, 0.29, 0.23, 0.25, 0.30, 0.08, 0.08, 0.05, 0.06, 0.06, 0.21, 0.19, 0.28, 1.00, 0.15, 0.00, 0.25,
        0.19, 0.23, 0.25, 0.20, 0.19, 0.14, 0.08, 0.07, 0.12, 0.06, 0.20, 0.10, 0.20, 0.15, 1.00, 0.00, 0.23,
        0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00,
        0.24, 0.56, 0.50, 0.53, 0.53, 0.25, 0.22, 0.14, 0.19, 0.11, 0.37, 0.23, 0.27, 0.25, 0.23, 0.00, 1.00
    };
    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());

    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed
    intraBucketCorrelation_[RiskType::Equity] = { 0.18, 0.23, 0.28, 0.27, 0.23, 0.36, 0.38,
        0.35, 0.21, 0.20, 0.54, 0.54 };

    // Commodity intra-bucket correlations
    intraBucketCorrelation_[RiskType::Commodity] = { 0.79, 0.98, 0.96, 0.97, 0.98, 0.88, 0.97, 0.42, 0.70, 
        0.38, 0.54, 0.48, 0.67, 0.15, 0.23, 0.00, 0.33 };

    // Initialise the single, ad-hoc type, correlations 
    xccyCorr_ = 0.07;
    infCorr_ = 0.41;
    infVolCorr_ = 0.41;
    irSubCurveCorr_ = 0.986;
    irInterCurrencyCorr_ = 0.22;
    crqResidualIntraCorr_ = 0.5;
    crqSameIntraCorr_ = 0.92;
    crqDiffIntraCorr_ = 0.41;
    crnqResidualIntraCorr_ = 0.5;
    crnqSameIntraCorr_ = 0.86;
    crnqDiffIntraCorr_ = 0.33;
    crnqInterCorr_ = 0.36;
    fxCorr_ = 0.5;
    basecorrCorr_ = 0.25;

    // clang-format on
}

/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)
is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)
of the ISDA-SIMM-v2.3.8 documentation).
*/
QuantLib::Real SimmConfiguration_ISDA_V2_3_8::curvatureMarginScaling() const { return pow(hvr_ir_, -2.0); }

void SimmConfiguration_ISDA_V2_3_8::addLabels2(const RiskType& rt, const string& label_2) {
    // Call the shared implementation
    SimmConfigurationBase::addLabels2Impl(rt, label_2);
}

string SimmConfiguration_ISDA_V2_3_8::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {
    // Special for BMA
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        return "Municipal";
    }

    // Otherwise pass off to base class
    return SimmConfigurationBase::labels2(irIndex);
}

} // namespace analytics
} // namespace ore
