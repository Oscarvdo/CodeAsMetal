#pragma once
/** Shared behavioral tests run standalone without dependencies or through GTest.
 * Expected results are independent hand calculations, not snapshots of code. */
#include "cam/Domain.h"
#include <functional>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <utility>
namespace tests {
inline void require(bool ok){if(!ok)throw std::runtime_error("Behavior assertion failed");}
inline void near(double actual,double expected){require(std::abs(actual-expected)<1e-8);}
inline void rejects(const std::function<void()>& action){try{action();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Expected invalid_argument");}
inline cam::CostInput completeInput(){
    cam::CostInput i;i.quantity=10;i.asOf="2026-09-16";i.material="TEST";
    i.stockMassKg=2;i.toolingPerPart=1;i.logisticsPerBatch=20;i.overheadPercent=10;
    i.rates={{"m","material:TEST","USD","2026-01-01","","Synthetic test",5},
      {"c","machine:M1","USD","2026-01-01","","Synthetic test",60},
      {"l","labor:operator","USD","2026-01-01","","Synthetic test",30},
      {"q","inspection:quality","USD","2026-01-01","","Synthetic test",60}};
    cam::Operation op;op.id="OP10";op.kind="Milling";op.machine="M1";op.minutes=6;op.timeKnown=true;op.setupMinutes=30;op.setupKnown=true;i.operations.push_back(op);
    op.id="OP20";op.kind="Inspection";op.minutes=3;op.setupMinutes=0;i.operations.push_back(op);return i;
}
inline const std::vector<std::pair<const char*,std::function<void()>>>& cases(){
    static const std::vector<std::pair<const char*,std::function<void()>>> c={
      {"KnownPlanCostsUsePerBatchSetupAndPerPartCycle",[]{auto e=cam::estimate(completeInput());require(e.complete);near(e.knownBatchTotal,324.5);near(e.knownUnitTotal,32.45);near(e.completeness,100);}},
      {"MissingRateDoesNotBecomeZero",[]{auto i=completeInput();i.rates.erase(i.rates.begin());auto e=cam::estimate(i);require(!e.complete);require(!e.lines[0].amount);require(!e.lines[7].amount);near(e.completeness,75);}},
      {"ExplicitZeroLogisticsIsKnown",[]{auto i=completeInput();i.logisticsPerBatch=0;auto e=cam::estimate(i);require(e.complete);near(*e.lines[6].amount,0);}},
      {"UnknownTimeInvalidatesMachineLaborAndOverhead",[]{auto i=completeInput();i.operations[0].timeKnown=false;auto e=cam::estimate(i);require(!e.lines[2].amount&&!e.lines[3].amount&&!e.lines[7].amount);}},
      {"SetupNotMultipliedByBatchSize",[]{auto i=completeInput();auto a=cam::estimate(i);i.quantity=100;auto b=cam::estimate(i);near(*a.lines[1].amount,45);near(*a.lines[1].amount,*b.lines[1].amount);}},
      {"AbsentInspectionRemainsUnknown",[]{auto i=completeInput();i.operations.pop_back();auto e=cam::estimate(i);require(!e.lines[5].amount&&!e.complete);}},
      {"EmptyPlanDoesNotClaimZeroManufacturingCost",[]{auto i=completeInput();i.operations.clear();auto e=cam::estimate(i);require(!e.lines[1].amount&&!e.lines[2].amount&&!e.lines[3].amount&&!e.lines[5].amount);}},
      {"RateIntervalIsHalfOpen",[]{auto r=completeInput().rates;r[0].to="2026-09-16";r.push_back({"m2","material:TEST","USD","2026-09-16","","Synthetic test",7});near(cam::rateAt(r,"material:TEST","2026-09-15","USD")->value,5);near(cam::rateAt(r,"material:TEST","2026-09-16","USD")->value,7);}},
      {"OverlappingRatesRejected",[]{auto r=completeInput().rates;auto x=r[0];x.id="other";r.push_back(x);rejects([&]{cam::validateRates(r);});}},
      {"WrongCurrencyIsMissing",[]{auto i=completeInput();i.currency="EUR";auto e=cam::estimate(i);require(!e.lines[0].amount&&!e.lines[2].amount);}},
      {"InvalidCalendarDateRejected",[]{auto i=completeInput();i.asOf="2026-02-29";rejects([&]{cam::estimate(i);});i.asOf="2024-02-29";cam::estimate(i);}},
      {"NonFiniteValuesRejected",[]{auto i=completeInput();i.stockMassKg=std::numeric_limits<double>::quiet_NaN();rejects([&]{cam::estimate(i);});}},
      {"NegativeTimeRejected",[]{auto i=completeInput();i.operations[0].minutes=-1;rejects([&]{cam::estimate(i);});}},
      {"InvalidQuantityRejected",[]{auto i=completeInput();i.quantity=0;rejects([&]{cam::estimate(i);});}},
      {"DfmThresholdBoundaryDoesNotWarn",[]{cam::Feature f;f.id="H1";f.kind="Through hole";f.diameter=6;f.depth=30;require(cam::evaluate({f},{}).empty());f.depth=30.1;require(cam::evaluate({f},{}).size()==1);}},
      {"DfmPreservesMeasuredEvidenceAndRuleVersion",[]{cam::Feature f;f.id="H1";f.kind="Hole candidate";f.diameter=6.35;f.depth=38.1;cam::Rules r;r.version="customer-v3";auto e=cam::evaluate({f},r);near(e[0].measured,6);require(e[0].version=="customer-v3"&&e[0].severity=="Review");}},
      {"SurfaceCylinderIsNotAutomaticallyHole",[]{cam::Feature f;f.kind="Cylindrical surface";f.diameter=1;f.depth=100;require(cam::evaluate({f},{}).empty());}},
      {"ThinWallAndRadiusScreening",[]{cam::Feature a;a.kind="Thin wall";a.width=0.5;cam::Feature b;b.kind="Pocket";b.radius=0.5;require(cam::evaluate({a,b},{}).size()==2);}},
      {"UnconfirmedFeaturesDoNotGenerateDrilling",[]{cam::Feature f;f.id="H1";f.kind="Through hole";require(cam::propose({f}).size()==2);f.confirmed=true;auto p=cam::propose({f});require(p.size()==3&&p[1].kind=="Drilling"&&!p[1].timeKnown);}},
      {"OverridePreservesRecommendationAndRequiresRationale",[]{cam::Operation o;o.recommendedMinutes=7;rejects([&]{cam::overrideTime(o,5,"","Engineer","UTC");});cam::overrideTime(o,5,"Measured trial","Engineer","2026-09-16T00:00:00Z");require(o.recommendedMinutes.has_value());near(*o.recommendedMinutes,7);near(o.minutes,5);require(o.timeKnown&&o.actor=="Engineer");}},
      {"RuleThresholdsMustBePositive",[]{cam::Rules r;r.maxHoleRatio=0;rejects([&]{cam::evaluate({},r);});}},
      {"OverflowIsRejected",[]{auto i=completeInput();i.stockMassKg=1e308;rejects([&]{cam::estimate(i);});}}
    };return c;
}
}
