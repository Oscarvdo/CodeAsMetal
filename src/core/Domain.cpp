#include "cam/Domain.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>

namespace cam {
namespace {
void nonnegative(double x, const char* field) {
    if (!std::isfinite(x) || x < 0) throw std::invalid_argument(std::string(field)+" must be finite and >= 0");
}
void optionalNonnegative(std::optional<double> x, const char* field) { if(x) nonnegative(*x,field); }
// Lexical comparison is safe only after validating ISO dates, including leap days.
bool dateValid(const std::string& s) {
    if(s.size()!=10 || s[4]!='-' || s[7]!='-') return false;
    for(std::size_t i=0;i<s.size();++i) if(i!=4 && i!=7 && (s[i]<'0'||s[i]>'9')) return false;
    const int y=std::stoi(s.substr(0,4)), m=std::stoi(s.substr(5,2)), d=std::stoi(s.substr(8,2));
    if(y<1 || m<1 || m>12) return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return d>=1 && d<=days[m-1]+(m==2 && y%4==0 && (y%100!=0 || y%400==0));
}
std::string number(double x) { std::ostringstream s; s.precision(12); s<<x; return s.str(); }
}
void validate(const Rules& r) {
    if(r.version.empty()) throw std::invalid_argument("Rule version is required");
    for(double x : {r.maxHoleRatio,r.minWall,r.minRadius})
        if(!std::isfinite(x)||x<=0) throw std::invalid_argument("Rule thresholds must be positive and finite");
}
void validateRates(const std::vector<Rate>& rates) {
    for(std::size_t i=0;i<rates.size();++i) {
        const auto& a=rates[i]; nonnegative(a.value,"Rate");
        if(a.id.empty()||a.key.empty()||a.source.empty()||a.currency.size()!=3||!dateValid(a.from)||
           (!a.to.empty() && (!dateValid(a.to)||a.to<=a.from)))
            throw std::invalid_argument("Rate requires ID, source, key, currency and valid date interval");
        for(std::size_t j=0;j<i;++j) {
            const auto& b=rates[j];
            if(a.id==b.id) throw std::invalid_argument("Duplicate rate ID");
            if(a.key==b.key && a.currency==b.currency &&
               (a.to.empty()||b.from<a.to) && (b.to.empty()||a.from<b.to))
                throw std::invalid_argument("Overlapping rate history: "+a.key);
        }
    }
}
std::optional<Rate> rateAt(const std::vector<Rate>& rates,const std::string& key,
                          const std::string& date,const std::string& currency) {
    validateRates(rates);
    if(!dateValid(date)) throw std::invalid_argument("Estimate date must be YYYY-MM-DD");
    for(const auto& r:rates) if(r.key==key && r.currency==currency && r.from<=date && (r.to.empty()||date<r.to)) return r;
    return {};
}
std::vector<Finding> evaluate(const std::vector<Feature>& features,const Rules& r) {
    validate(r); std::vector<Finding> out;
    for(const auto& f:features) {
        for(double x:{f.diameter,f.depth,f.width,f.radius}) nonnegative(x,"Feature measurement");
        const auto add=[&](std::string rule,double measured,double threshold,std::string why) {
            out.push_back({f.id,rule,r.version,f.confirmed?"Warning":"Review",why,measured,threshold});
        };
        if((f.kind=="Through hole"||f.kind=="Blind hole"||f.kind=="Hole candidate") && f.diameter>0 && f.depth/f.diameter>r.maxHoleRatio)
            add("DRILL-HOLE-004",f.depth/f.diameter,r.maxHoleRatio,"Depth / diameter exceeds configured screening ratio; verify tool reach and chip evacuation.");
        if(f.kind=="Thin wall" && f.width>0 && f.width<r.minWall)
            add("MILL-WALL-001",f.width,r.minWall,"Wall thickness below configured minimum; review rigidity and workholding.");
        if((f.kind=="Fillet"||f.kind=="Pocket"||f.kind=="Slot") && f.radius>0 && f.radius<r.minRadius)
            add("MILL-RADIUS-001",f.radius,r.minRadius,"Internal radius below configured minimum tool radius.");
    }
    return out;
}
Estimate estimate(const CostInput& in) {
    if(in.quantity<1 || in.quantity>100000000) throw std::invalid_argument("Quantity outside 1..100000000");
    if(in.currency.size()!=3) throw std::invalid_argument("Use a three-letter currency");
    validateRates(in.rates);
    if(!dateValid(in.asOf)) throw std::invalid_argument("Invalid estimate date");
    optionalNonnegative(in.stockMassKg,"Stock mass"); optionalNonnegative(in.toolingPerPart,"Tooling");
    optionalNonnegative(in.logisticsPerBatch,"Logistics"); optionalNonnegative(in.overheadPercent,"Overhead");
    Estimate out;
    auto line=[&](std::string category,std::optional<double> amount,std::string basis) {
        if(amount) nonnegative(*amount,"Computed amount");
        out.lines.push_back({std::move(category),std::move(basis),amount});
    };
    auto get=[&](const std::string& key){return rateAt(in.rates,key,in.asOf,in.currency);};
    auto rateBasis=[](const Rate& r){return "rate="+r.id+"; source="+r.source+"; value="+number(r.value)+"; effective="+r.from+".."+r.to;};
    auto mat=get("material:"+in.material);
    line("Material",mat&&in.stockMassKg?std::optional{mat->value * *in.stockMassKg * in.quantity}:std::nullopt,
         mat&&in.stockMassKg?number(*in.stockMassKg)+" kg stock/part * "+number(in.quantity)+" * "+rateBasis(*mat):"Missing stock mass or material price/kg");
    double setup=0,machine=0,labor=0,inspection=0;
    bool setupOk=!in.operations.empty(), machineOk=setupOk,laborOk=setupOk,inspectOk=setupOk;
    std::string sb,mb,lb,ib;
    bool hasManufacturing=false,hasInspection=false;
    const auto lr=get("labor:"+in.labor), ir=get("inspection:"+in.inspector);
    for(const auto& op:in.operations) {
        nonnegative(op.minutes,"Cycle minutes"); nonnegative(op.setupMinutes,"Setup minutes");
        const auto mr=get("machine:"+op.machine);
        if(op.kind=="Inspection") {
            hasInspection=true;
            if(!op.timeKnown || !ir) inspectOk=false;
            else { inspection+=op.minutes/60*ir->value*in.quantity; ib+=op.id+": "+number(op.minutes)+" min/part; "+rateBasis(*ir)+"; "; }
        } else {
            hasManufacturing=true;
            if(!op.timeKnown || !mr) machineOk=false;
            else {machine+=op.minutes/60*mr->value*in.quantity; mb+=op.id+": "+number(op.minutes)+" min/part; "+rateBasis(*mr)+"; ";}
            if(!op.timeKnown || !lr) laborOk=false;
            else {labor+=op.minutes/60*lr->value*in.quantity; lb+=op.id+": 100% attended "+number(op.minutes)+" min/part; "+rateBasis(*lr)+"; ";}
        }
        if(!op.setupKnown) setupOk=false;
        else if(op.setupMinutes>0) {
            if(!mr||!lr) setupOk=false;
            else {setup+=op.setupMinutes/60*(mr->value+lr->value); sb+=op.id+": "+number(op.setupMinutes)+" min/batch * ("+rateBasis(*mr)+" + "+rateBasis(*lr)+"); ";}
        }
    }
    machineOk=machineOk&&hasManufacturing; laborOk=laborOk&&hasManufacturing; inspectOk=inspectOk&&hasInspection;
    auto amount=[](bool ok,double x)->std::optional<double>{return ok?std::optional{x}:std::nullopt;};
    line("Setup",amount(setupOk,setup),setupOk?sb:"Missing setup time or setup machine/labor rate");
    line("Machine",amount(machineOk,machine),machineOk?mb:"Missing cycle time or machine rate");
    line("Labor",amount(laborOk,labor),laborOk?lb:"Missing cycle time or operator rate");
    line("Tooling",in.toolingPerPart?std::optional{*in.toolingPerPart*in.quantity}:std::nullopt,"Configured tooling/part * quantity; blank = missing");
    line("Inspection",amount(inspectOk,inspection),inspectOk?ib:"Missing inspection time/rate or plan");
    line("Logistics",in.logisticsPerBatch,"Configured logistics for whole batch; blank = missing");
    double base=0; bool all=true;
    for(const auto& l:out.lines) {if(l.amount) base+=*l.amount; else all=false;}
    line("Overhead",all&&in.overheadPercent?std::optional{base * *in.overheadPercent/100}:std::nullopt,
         "Configured percentage applied to all seven direct categories; requires complete direct subtotal");
    int known=0;
    for(const auto& l:out.lines) if(l.amount) {out.knownBatchTotal+=*l.amount; ++known;}
    nonnegative(out.knownBatchTotal,"Total");
    out.knownUnitTotal=out.knownBatchTotal/in.quantity;
    out.completeness=100.0*known/out.lines.size(); out.complete=known==static_cast<int>(out.lines.size());
    return out;
}
std::vector<Operation> propose(const std::vector<Feature>& features) {
    std::vector<Operation> out;
    Operation rough; rough.id="OP10"; rough.kind="Milling"; rough.setup="SETUP-1";
    rough.basis="Preliminary stock preparation / facing. Engineer supplies stock, machine, tool and times."; out.push_back(rough);
    for(const auto& f:features) if(f.confirmed && (f.kind=="Through hole"||f.kind=="Blind hole"||f.kind=="Pocket"||f.kind=="Slot"||f.kind=="Chamfer")) {
        Operation op; op.id="OP"+std::to_string((out.size()+1)*10);
        op.kind=(f.kind=="Through hole"||f.kind=="Blind hole")?"Drilling":"Milling";
        op.setup="REVIEW"; op.featureIds=f.id; op.basis="Suggested from confirmed "+f.kind+"; tool access and setup require review."; out.push_back(op);
    }
    Operation inspect; inspect.id="OP"+std::to_string((out.size()+1)*10); inspect.kind="Inspection";
    inspect.basis="Inspection scope requires drawing/tolerances; time unknown."; out.push_back(inspect); return out;
}
void overrideTime(Operation& op,double minutes,const std::string& reason,const std::string& actor,const std::string& utc) {
    nonnegative(minutes,"Override minutes");
    if(reason.empty()||actor.empty()||utc.empty()) throw std::invalid_argument("Time override requires reason, actor and timestamp");
    op.minutes=minutes;op.timeKnown=true;op.overrideReason=reason;op.actor=actor;op.changedUtc=utc;
}
}
