#pragma once
/** @file Pure engineering model. Length mm, volume mm^3, time min, mass kg.
 * UI, CAD and database objects must not cross this boundary. Missing values are
 * optional; zero means explicitly supplied zero, never unknown. */
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cam {
struct Geometry {
    std::array<double, 3> size{}, center{};
    double area{};
    std::optional<double> volume; // Only closed, valid, single STEP solids qualify.
    int solids{}, shells{}, faces{}, edges{}, vertices{};
    bool mesh{}, valid{};
};
/** Face identifiers are scoped to a CAD SHA-256 and analysis version, not revisions. */
struct Feature {
    std::string id, kind, evidence;
    std::vector<int> faces;
    double diameter{}, depth{}, width{}, radius{};
    std::array<double, 3> direction{};
    bool confirmed{}; // Recognition candidates require engineering confirmation.
};
struct Rules {
    std::string version{"1.0"};
    double maxHoleRatio{5}, minWall{1}, minRadius{1};
};
struct Finding {
    std::string featureId, rule, version, severity, explanation;
    double measured{}, threshold{};
};
/** An operation records both recommendation and the engineer's effective time.
 * Cycle time is per part; setup time is per batch. Machine rates exclude labor. */
struct Operation {
    std::string id, kind, setup, machine, tool, featureIds, basis;
    std::optional<double> recommendedMinutes; // No physical time model is enabled yet.
    double minutes{};
    bool timeKnown{};
    double setupMinutes{};
    bool setupKnown{};
    std::string overrideReason, actor, changedUtc;
};
/** Effective-dated generic customer rate. key examples: material:6061,
 * machine:VMC01, labor:operator, inspection:quality. Dates are ISO YYYY-MM-DD.
 * Windows are [from,to). Empty to means no end. No mixed-currency conversion. */
struct Rate {
    std::string id, key, currency, from, to, source;
    double value{};
};
struct CostInput {
    int quantity{1};
    std::string currency{"USD"}, asOf, material, labor{"operator"}, inspector{"quality"};
    std::optional<double> stockMassKg, toolingPerPart, logisticsPerBatch, overheadPercent;
    std::vector<Operation> operations;
    std::vector<Rate> rates;
};
struct CostLine {
    std::string category, basis;
    std::optional<double> amount; // Whole batch amount, in selected currency.
};
struct Estimate {
    std::vector<CostLine> lines;
    double knownBatchTotal{}, knownUnitTotal{}, completeness{};
    bool complete{};
    std::string modelVersion{"cost-1.0"};
};
/** Validate numerical and interval invariants; throw invalid_argument on violation. */
void validate(const Rules&);
void validateRates(const std::vector<Rate>&);
/** Bounded DFM screening only. No machining feasibility or standards certification. */
std::vector<Finding> evaluate(const std::vector<Feature>&, const Rules&);
/** Pick one historical rate. Overlaps are errors, not implicit last-write-wins. */
std::optional<Rate> rateAt(const std::vector<Rate>&, const std::string& key,
                         const std::string& date, const std::string& currency);
/** Cost a reviewed operation plan. Completeness is known category count / 8,
 * not confidence or statistical accuracy. Unknown inputs never become zero. */
Estimate estimate(const CostInput&);
/** Seed a preliminary plan with UNKNOWN times. No synthetic MRR or shop rates. */
std::vector<Operation> propose(const std::vector<Feature>&);
/** Check and stamp a time override, retaining the original recommendation. */
void overrideTime(Operation&, double minutes, const std::string& reason,
                  const std::string& actor, const std::string& utc);
}
