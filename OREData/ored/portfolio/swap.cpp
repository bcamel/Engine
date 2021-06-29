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

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <ql/time/calendars/target.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/currencyswap.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::make_pair;

namespace ore {
namespace data {

void Swap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Swap::build() called for trade " << id());

    QL_REQUIRE(legData_.size() >= 1, "Swap must have at least 1 leg");
    const boost::shared_ptr<Market> market = engineFactory->market();

    // allow minor currencies in case first leg is equity
    Currency currency = parseCurrencyWithMinors(legData_[0].currency());
    string ccy_str = currency.code();

    Size numLegs = legData_.size();
    legPayers_ = vector<bool>(numLegs);
    std::vector<QuantLib::Currency> currencies(numLegs);
    legs_.resize(numLegs);

    isXCCY_ = false;
    isResetting_ = false;

    for (Size i = 0; i < numLegs; ++i) {
        // allow minor currencies for Equity legs as some exchanges trade in these, e.g LSE in pence - GBX or GBp
        // minor currencies on other legs will fail here
        if (legData_[i].legType() == "Equity")
            currencies[i] = parseCurrencyWithMinors(legData_[i].currency());
        else
            currencies[i] = parseCurrency(legData_[i].currency());
        if (currencies[i] != currency)
            isXCCY_ = true;
        isResetting_ = isResetting_ || (!legData_[i].isNotResetXCCY());
    }

    boost::shared_ptr<EngineBuilder> builder =
        isXCCY_ ? engineFactory->builder("CrossCurrencySwap") : engineFactory->builder("Swap");
    auto configuration = builder->configuration(MarketContext::pricing);

    for (Size i = 0; i < numLegs; ++i) {
        legPayers_[i] = legData_[i].isPayer();
        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        legs_[i] = legBuilder->buildLeg(legData_[i], engineFactory, requiredFixings_, configuration);
        DLOG("Swap::build(): currency[" << i << "] = " << currencies[i]);
        
        // add notional leg, if appicable
        auto leg = buildNotionalLeg(legData_[i], legs_[i], requiredFixings_, engineFactory->market(), configuration);
        if (!leg.empty()) {
            legs_.push_back(leg);
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);
        }
    } // for legs

    // The npv currency, notional currency and current notional are taken from the first leg that
    // appears in the XML that has a notional. If no such leg exists the notional currency
    // and current notional are left empty and the npv currency is set to the first leg's currency

    notionalTakenFromLeg_ = 0;
    for (; notionalTakenFromLeg_ < legData_.size(); ++notionalTakenFromLeg_) {
        const LegData& d = legData_[notionalTakenFromLeg_];
        if (!d.notionals().empty())
            break;
    }

    if (notionalTakenFromLeg_ == legData_.size()) {
        ALOG("no suitable leg found to set notional, set to null and notionalCurrency to empty string");
        notional_ = Null<Real>();
        notionalCurrency_ = "";
        // parse for currrency in case first leg is Equity, we only want the major currency for NPV
        npvCurrency_ = parseCurrencyWithMinors(legData_.front().currency()).code();
    } else {
        if (legData_[notionalTakenFromLeg_].schedule().hasData()) {
            Schedule schedule = makeSchedule(legData_[notionalTakenFromLeg_].schedule());
            auto notional =
                buildScheduledVectorNormalised(legData_[notionalTakenFromLeg_].notionals(),
                                               legData_[notionalTakenFromLeg_].notionalDates(), schedule, 0.0);
            Date today = Settings::instance().evaluationDate();
            auto d = std::upper_bound(schedule.dates().begin(), schedule.dates().end(), today);
            // forward starting => take first notional
            // on or after last schedule date => zero notional
            // in between => notional of current period
            if (d == schedule.dates().begin())
                notional_ = notional.at(0);
            else if (d == schedule.dates().end())
                notional_ = 0.0;
            else
                notional_ = notional.at(std::distance(schedule.dates().begin(), d) - 1);
        } else {
            notional_ = legData_[notionalTakenFromLeg_].notionals().at(0);
        }
        // parse for currrency in case leg is Equity, we only want the major currency for NPV and Notional
        notionalCurrency_ = parseCurrencyWithMinors(legData_[notionalTakenFromLeg_].currency()).code();
        npvCurrency_ = parseCurrencyWithMinors(legData_[notionalTakenFromLeg_].currency()).code();
        DLOG("Notional is " << notional_ << " " << notionalCurrency_);
    }

    Currency npvCcy = parseCurrency(npvCurrency_);
    DLOG("npv currency is " << npvCurrency_);

    if (isXCCY_) {
        boost::shared_ptr<QuantExt::CurrencySwap> swap(
            new QuantExt::CurrencySwap(legs_, legPayers_, currencies, settlement_ == "Physical", isResetting_));
        boost::shared_ptr<CrossCurrencySwapEngineBuilderBase> swapBuilder =
            boost::dynamic_pointer_cast<CrossCurrencySwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for CrossCurrencySwap " << id());
        swap->setPricingEngine(swapBuilder->engine(currencies, npvCcy));
        // take the first legs currency as the npv currency (arbitrary choice)
        instrument_.reset(new VanillaInstrument(swap));
    } else {
        boost::shared_ptr<QuantLib::Swap> swap(new QuantLib::Swap(legs_, legPayers_));
        boost::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            boost::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for Swap " << id());
        swap->setPricingEngine(swapBuilder->engine(npvCcy));
        instrument_.reset(new VanillaInstrument(swap));
    }

    DLOG("Set instrument wrapper");

    // set Leg Currencies
    legCurrencies_ = vector<string>(currencies.size());
    for (Size i = 0; i < currencies.size(); i++)
        legCurrencies_[i] = currencies[i].code();

    // set maturity
    maturity_ = Date::minDate();
    for (auto const& l : legs_) {
        if (!l.empty())
            maturity_ = std::max(maturity_, l.back()->date());
    }

}

const std::map<std::string,boost::any>& Swap::additionalData() const {
    Size numLegs = legData_.size();
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    for (Size i = 0; i < numLegs; ++i) {
        string legID = to_string(i+1);
        additionalData_["legType[" + legID + "]"] = legData_[i].legType();
        additionalData_["isPayer[" + legID + "]"] = legData_[i].isPayer();
        additionalData_["notionalCurrency[" + legID + "]"] = legData_[i].currency();
        for (Size j = 0; j < legs_[i].size(); ++j) {
            boost::shared_ptr<CashFlow> flow = legs_[i][j];
            // pick flow with earliest future payment date on this leg
            if (flow->date() > asof) {
                additionalData_["amount[" + legID + "]"] = flow->amount();
                additionalData_["paymentDate[" + legID + "]"] = to_string(flow->date());
                boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(flow);
                if (coupon) {
                    additionalData_["currentNotional[" + legID + "]"] = coupon->nominal();
                    additionalData_["rate[" + legID + "]"] = coupon->rate();
                    boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(flow);
                    if (frc) {
                        additionalData_["index[" + legID + "]"] = frc->index()->name();
                        additionalData_["spread[" + legID + "]"] = frc->spread();                        
                    }
                }
                break;
            }
        }
        if (legs_[i].size() > 0) {
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[i][0]);
            if (coupon)
                additionalData_["originalNotional[" + legID + "]"] = coupon->nominal();
        }
    }
    return additionalData_;
}

QuantLib::Real Swap::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        WLOG("swap engine does not provide current notional: " << e.what() << ", using fallback");
        // Try getting current notional from coupons
        if (notionalTakenFromLeg_ < legs_.size()) {
            Real n = currentNotional(legs_[notionalTakenFromLeg_]);
            if (fabs(n) > QL_EPSILON) {
                return n;
            }
        }
        // else return the face value
        WLOG("swap does not provide coupon notionals, useing face value");
        return notional_;
    }
}

std::string Swap::notionalCurrency() const {
    // try to get the notional ccy from the additional results of the instrument
    try {
        return instrument_->qlInstrument()->result<std::string>("notionalCurrency");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "notionalCurrency not provided"))
            WLOG("swap engine does not provide notional ccy: " << e.what() << ", using fallback");
        return notionalCurrency_;
    }
}

map<AssetClass, set<string>>
Swap::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

    map<AssetClass, set<string>> result;
    for (const auto& ld : legData_) {
        for (auto ind : ld.indices()) {
            // only handle equity and commodity for now
            if (ind.substr(0, 5) != "COMM-" && ind.substr(0, 3) != "EQ-")
                continue;

            boost::shared_ptr<Index> index = parseIndex(ind);

            if (auto ei = boost::dynamic_pointer_cast<EquityIndex>(index)) {
                result[AssetClass::EQ].insert(ei->name());
            } else if (auto ci = boost::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[AssetClass::COM].insert(ci->name());
            }
        }
    }

    return result;
}

void Swap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    legData_.clear();
    XMLNode* swapNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    // backwards compatibility
    if (swapNode == nullptr) {
        swapNode = XMLUtils::getChildNode(node, "SwapData");
    }
    QL_REQUIRE(swapNode, "Swap::fromXML(): expected '" << tradeType() << "Data'"
                                                       << (tradeType() == "Swap" ? "" : " or 'SwapData'"));

    settlement_ = XMLUtils::getChildValue(swapNode, "Settlement", false);
    if (settlement_ == "")
        settlement_ = "Physical";

    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        auto ld = createLegData();
        ld->fromXML(nodes[i]);
        legData_.push_back(*boost::static_pointer_cast<LegData>(ld));
    }
}

boost::shared_ptr<LegData> Swap::createLegData() const { return boost::make_shared<LegData>(); }

XMLNode* Swap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, swapNode);

    if (settlement_ == "Cash")
        XMLUtils::addChild(doc, swapNode, "Settlement", settlement_);
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swapNode, legData_[i].toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
