/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef PLAYER_AI_POLICY_H
#define PLAYER_AI_POLICY_H


/**
 * Decisions of an AI read from a key=value file given by -ai_policy or
 * -ai_policy_passenger (with -ai_soak); tools/ai_learn.py fits them.  Without
 * a file the AI keeps its hand-written choices.
 *
 * A candidate is a link the AI does not serve yet: supplier -> consumer for
 * one good (goods AI), or one town -> another (passenger AI).  Its score is
 * bias + the weighted features below; the AI builds the best candidate if its
 * score is above zero.  -ai_soak logs the same features for every link the
 * AI tries, policy or not.
 *
 * The weights are doubles: a policy only runs in single-player soaks, so
 * floating point cannot desync a network game.
 */
struct ai_policy_t
{
	/// supplier -> consumer for one good
	enum feature_t {
		F_REVENUE,      ///< log(1 + value * production * distance)
		F_DISTANCE,     ///< distance / 100 tiles
		F_LOG_DISTANCE, ///< log(1 + distance)
		F_DENSITY,      ///< log(1 + value * production): revenue per tile of way
		F_PRODUCTION,   ///< log(1 + monthly production)
		F_MARGIN,       ///< log(capacity * value / running cost) of the road vehicle for the good
		F_LOADS,        ///< log(1 + monthly production / its capacity): full loads a month
		F_RAW,          ///< supplier needs no input
		F_STOCK,        ///< supplier has goods waiting now
		F_END,          ///< consumer makes nothing (end of a chain)
		F_DOWNSTREAM,   ///< consumer makes goods that someone moved on last month
		F_SERVED,       ///< one of our stops already serves supplier or consumer
		F_WATER,        ///< supplier is on water, so the link needs ships
		FEATURES
	};
	/// one town -> another
	enum passenger_feature_t {
		P_POPULATION_FROM, ///< log(1 + inhabitants) of the first town
		P_POPULATION_TO,   ///< log(1 + inhabitants) of the second
		P_DISTANCE,        ///< distance / 100 tiles
		P_LOG_DISTANCE,    ///< log(1 + distance)
		P_NEW_HUBS,        ///< towns of the two without our stop yet
		P_ACROSS_WATER,    ///< no path over land joins the towns
		PASSENGER_FEATURES
	};
	static_assert( (int)PASSENGER_FEATURES <= (int)FEATURES, "weight[] holds either set" );
	/// the names in policy files ("w_" + name) and in the -ai_soak log
	static const char *const feature_names[FEATURES];
	static const char *const passenger_feature_names[PASSENGER_FEATURES];

	/// the features this policy weighs: feature_names or passenger_feature_names
	const char *const *names;
	int count;

	double bias;
	double weight[FEATURES];

	/// chance to build a random candidate instead of the best one, so that a
	/// soak sees links its policy would not choose (uses the game's rng)
	double explore;
	/// build only while the account holds this share of the starting money
	double cash_reserve;
	/// retire a convoy older than a year whose last 12 months lost more than
	/// retire_loss times its value (0: only the AI's own rules)
	double retire_loss;
	/// close a line older than a year whose convoys earned less than
	/// retire_line times the estimated upkeep of its stops and road (0: never)
	double retire_line;
	/// build or upgrade the headquarters only while the account holds this
	/// many times the starting money; its upkeep can eat a small company
	double hq_reserve;

	ai_policy_t(const char *const *names, int count);

	double score(const double *features) const;

	/// The policies from -ai_policy and -ai_policy_passenger, or NULL.
	static const ai_policy_t *goods;
	static const ai_policy_t *passengers;

	/// -ai_soak: log the links the AIs try (ai_line lines)
	static bool trace;

	/// Reads FILENAME into a policy over NAMES; keys left out keep their
	/// defaults.  Fatal if the file cannot be read or holds an unknown key.
	static const ai_policy_t *load(const char *filename, const char *const *names, int count);
};

#endif
