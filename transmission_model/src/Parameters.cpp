/*
 * Parameters.cpp
 *
 *  Created on: Jun 5, 2012
 *      Author: nick
 */

#include "boost/algorithm/string.hpp"

#include "repast_hpc/Utilities.h"
#include "repast_hpc/Properties.h"

#include "Parameters.h"

using namespace repast;


namespace TransModel {

const std::string RUN_NUMBER = "run";

const std::string NET_VAR = "net.variable.name";
const std::string CASUAL_NET_VAR = "casual.net.variable.name";

const std::string STOP_AT = "stop.at";
const std::string R_PARAMETERS_DERIVED = "r.parameters.derived";
const std::string R_PARAMETERS_NON_DERIVED = "r.parameters.non.derived";
const std::string R_FILE = "r.file";
const std::string MAIN_NETWORK_FILE = "main.network.file";
const std::string CASUAL_NETWORK_FILE = "casual.network.file";
const std::string EVENT_FILE = "event.file";
const std::string EVENT_FILE_BUFFER_SIZE = "event.file.buffer.size";
const std::string COUNTS_PER_TIMESTEP_OUTPUT_FILE= "per.tick.counts.output.file";
const std::string OUTPUT_DIR = "output.directory";
const std::string PARTNERSHIP_EVENTS_FILE = "partnership.events.file";
const std::string INFECTION_EVENTS_FILE =  "infection.events.file";
const std::string BIOMARKER_FILE = "biomarker.log.file";
const std::string BIOMARKER_LOG_COUNT = "biomarker.number.of.persons.to.log";
const std::string DEATH_EVENT_FILE = "death.events.file";
const std::string TESTING_EVENT_FILE = "testing.events.file";
const std::string ART_EVENT_FILE = "art.events.file";
const std::string PREP_EVENT_FILE = "prep.events.file";
const std::string PERSON_DATA_FILE = "person.data.file";
const std::string NET_SAVE_FILE = "net.save.file";
const std::string CASUAL_NET_SAVE_FILE = "casual.net.save.file";
const std::string NET_SAVE_AT = "save.network.at";
const std::string COUNT_OVERLAPS = "count.overlaps";

//const std::string PREP_MULT = "prep.mult";

// generated from parameters.R
const std::string ACUTE_LENGTH_MIN = "acute.length.min";
const std::string ACUTE_LENGTH_MAX = "acute.length.max";
const std::string ACUTE_MULT = "acute.mult";
const std::string ACUTE_MULT_HOLLING = "acute.mult.holling";
const std::string ART_INIT_TIME = "art.init.time";
const std::string B1_REF = "b1.ref";
const std::string B2_AFRICAN = "b2.african";
const std::string B3_FEMALE = "b3.female";
const std::string B4_CD4_REF = "b4.cd4.ref";
const std::string B5_AFRICAN = "b5.african";
const std::string B6_AGE_15TO29 = "b6.age.15to29";
const std::string B6_AGE_30TO39 = "b6.age.30to39";
const std::string B6_AGE_40TO49 = "b6.age.40to49";
const std::string B6_AGE_50ORMORE = "b6.age.50ormore";
const std::string CD4_AT_INFECTION_MALE = "cd4.at.infection.male";
const std::string CD4_REC_PER_TIMESTEP = "cd4.rec.per.timestep";
const std::string CD4_RECOVERY_TIME = "cd4.recovery.time";
const std::string CHRONIC_LENGTH_MIN = "chronic.length.min";
const std::string CHRONIC_LENGTH_MAX = "chronic.length.max";
const std::string CIRCUM_MULT = "circum.mult";
const std::string CIRCUM_RATE = "circum.rate";
const std::string DAILY_ENTRY_RATE = "daily.entry.rate";
const std::string DAILY_TESTING_PROB = "daily.testing.prob";
const std::string DETECTION_WINDOW = "detection.window";
const std::string DUR_INF = "dur.inf";
const std::string DURATION_OF_INFECTION = "duration.of.infection";
const std::string DUR_INF_BY_AGE = "given.dur.inf.by.age";
const std::string INIT_HIV_PREV = "init.hiv.prev";
const std::string INIT_HIV_PREV_ENTRIES = "init.hiv.prev.for.entries";
const std::string INFECTIVE_INSERTIVE_MULT = "inf.part.insertive.mult";
const std::string LATE_LENGTH_MIN = "late.length.min";
const std::string LATE_LENGTH_MAX = "late.length.max";
const std::string LATE_MULT = "late.mult";
const std::string LATE_MULT_HOLLING = "late.mult.holling";
const std::string LATE_STAGE_VIRAL_LOAD = "late.stage.viral.load";
const std::string MAX_AGE = "max.age";
const std::string MIN_AGE = "min.age";
const std::string MIN_CHRONIC_INFECTIVITY_UNADJ = "min.chronic.infectivity.unadj";
const std::string NON_TESTERS_PROP = "non.testers.prop";
const std::string NUM_SEX_ACTS_PER_TIMESTEP = "num.sex.acts.per.timestep";
const std::string PEAK_VIRAL_LOAD = "peak.viral.load";
const std::string PER_DAY_CD4_RECOVERY = "per.day.cd4.recovery";
const std::string PREG_MULT = "preg.mult";
const std::string PREG_SUSC_MULT = "preg.susc.mult";
const std::string PREP_TRANS_REDUCTION = "prep.transm.red";
const std::string PREP_USE_PROP = "prep.bl.use.prop";
const std::string PREP_DAILY_STOP_PROB = "prep.daily.stop.prob";
const std::string SET_POINT_VIRAL_LOAD = "set.point.viral.load";
const std::string SIZE_OF_TIMESTEP = "size.of.timestep";
const std::string TIME_INFECTION_TO_LATE_STAGE = "time.infection.to.late.stage";
const std::string TIME_INFECTION_TO_PEAK_VIRAL_LOAD = "time.infection.to.peak.viral.load";
const std::string TIME_INFECTION_TO_PEAK_VIREMIA = "time.infection.to.peak.viremia";
const std::string TIME_INFECTION_TO_VIRAL_SET_POINT = "time.infection.to.viral.set.point";
const std::string TIME_TO_FULL_SUPP = "time.to.full.supp";
const std::string UNDETECTABLE_VL = "undetectable.vl";
const std::string UNINFECTED_CD4_LEVEL = "uninfected.cd4.level";
const std::string UNTREATED_CD4_DAILY_DECLINE = "untreated.cd4.daily.decline";
const std::string UNTREATED_CD4_PERSTEP_DECLINE = "untreated.cd4.perstep.decline";
const std::string VIRAL_LOAD_LOG_INCREMENT = "inf.inc.log.vl";

const std::string PR_INSERTIVE_MAIN = "pr_insertive_main";
const std::string PR_RECEPTIVE_MAIN = "pr_receptive_main";

const std::string PR_INSERTIVE_CASUAL = "pr_insertive_casual";
const std::string PR_RECEPTIVE_CASUAL = "pr_receptive_casual";

const std::string PROP_STEADY_SEX_ACTS = "prop.steady.sex.acts";
const std::string PROP_CASUAL_SEX_ACTS = "prop.casual.sex.acts";
const std::string INFECTIVITY_REDUCTION_CONDOM = "inf.red.w.condom";

const std::string ART_LAG_PREFIX = "art.init.lag";
const std::string ASM_PREFIX = "asm.";

const std::string PARTIAL_ART_ADHER_WINDOW_LENGTH = "partial.art_adher.window.length";
const std::string PROP_NEVER_ADHERENT = "prop.never.adherent";
const std::string PROP_ALWAYS_ADHERENT = "prop.always.adherent";
const std::string PROP_PARTIAL_POS_ADHERENT = "prop.part.plus.adherent";
const std::string PROP_PARTIAL_NEG_ADHERENT = "prop.part.neg.adherent";

const std::string ALWAYS_ADHERENT_PROB = "always.adherent.probability";
const std::string NEVER_ADHERENT_PROB = "never.adherent.probability";
const std::string PARTIAL_POS_ADHERENT_PROB = "partial.pos.adherent.probability";
const std::string PARTIAL_NEG_ADHERENT_PROB = "partial.neg.adherent.probability";


const std::string SD_STEADY_NEVER_USE_CONDOMS = "sd.steady.never.use.condoms";
const std::string SD_STEADY_RARELY_USE_CONDOMS = "sd.steady.rarely.use.condoms";
const std::string SD_STEADY_SOMETIMES_USE_CONDOMS ="sd.steady.sometimes.use.condoms";
const std::string SD_STEADY_USUALLY_USE_CONDOMS = "sd.steady.usually.use.condoms";
const std::string SD_STEADY_ALWAYS_USE_CONDOMS = "sd.steady.always.use.condoms";

const std::string SD_CASUAL_NEVER_USE_CONDOMS = "sd.casual.never.use.condoms";
const std::string SD_CASUAL_RARELY_USE_CONDOMS = "sd.casual.rarely.use.condoms";
const std::string SD_CASUAL_SOMETIMES_USE_CONDOMS = "sd.casual.sometimes.use.condoms";
const std::string SD_CASUAL_USUALLY_USE_CONDOMS = "sd.casual.usually.use.condoms";
const std::string SD_CASUAL_ALWAYS_USE_CONDOMS = "sd.casual.always.use.condoms";

const std::string SC_STEADY_NEVER_USE_CONDOMS = "sc.steady.never.use.condoms";
const std::string SC_STEADY_RARELY_USE_CONDOMS = "sc.steady.rarely.use.condoms";
const std::string SC_STEADY_SOMETIMES_USE_CONDOMS ="sc.steady.sometimes.use.condoms";
const std::string SC_STEADY_USUALLY_USE_CONDOMS = "sc.steady.usually.use.condoms";
const std::string SC_STEADY_ALWAYS_USE_CONDOMS = "sc.steady.always.use.condoms";

const std::string SC_CASUAL_NEVER_USE_CONDOMS = "sc.casual.never.use.condoms";
const std::string SC_CASUAL_RARELY_USE_CONDOMS = "sc.casual.rarely.use.condoms";
const std::string SC_CASUAL_SOMETIMES_USE_CONDOMS = "sc.casual.sometimes.use.condoms";
const std::string SC_CASUAL_USUALLY_USE_CONDOMS = "sc.casual.usually.use.condoms";
const std::string SC_CASUAL_ALWAYS_USE_CONDOMS = "sc.casual.always.use.condoms";

const std::string SD_STEADY_NEVER_USE_CONDOMS_PROB = "sd.steady.never.use.condoms.prob";
const std::string SD_STEADY_RARELY_USE_CONDOMS_PROB = "sd.steady.rarely.use.condoms.prob";
const std::string SD_STEADY_SOMETIMES_USE_CONDOMS_PROB ="sd.steady.sometimes.use.condoms.prob";
const std::string SD_STEADY_USUALLY_USE_CONDOMS_PROB = "sd.steady.usually.use.condoms.prob";
const std::string SD_STEADY_ALWAYS_USE_CONDOMS_PROB = "sd.steady.always.use.condoms.prob";

const std::string SD_CASUAL_NEVER_USE_CONDOMS_PROB = "sd.casual.never.use.condoms.prob";
const std::string SD_CASUAL_RARELY_USE_CONDOMS_PROB = "sd.casual.rarely.use.condoms.prob";
const std::string SD_CASUAL_SOMETIMES_USE_CONDOMS_PROB = "sd.casual.sometimes.use.condoms.prob";
const std::string SD_CASUAL_USUALLY_USE_CONDOMS_PROB = "sd.casual.usually.use.condoms.prob";
const std::string SD_CASUAL_ALWAYS_USE_CONDOMS_PROB = "sd.casual.always.use.condoms.prob";

const std::string SC_STEADY_NEVER_USE_CONDOMS_PROB = "sc.steady.never.use.condoms.prob";
const std::string SC_STEADY_RARELY_USE_CONDOMS_PROB = "sc.steady.rarely.use.condoms.prob";
const std::string SC_STEADY_SOMETIMES_USE_CONDOMS_PROB ="sc.steady.sometimes.use.condoms.prob";
const std::string SC_STEADY_USUALLY_USE_CONDOMS_PROB = "sc.steady.usually.use.condoms.prob";
const std::string SC_STEADY_ALWAYS_USE_CONDOMS_PROB = "sc.steady.always.use.condoms.prob";

const std::string SC_CASUAL_NEVER_USE_CONDOMS_PROB = "sc.casual.never.use.condoms.prob";
const std::string SC_CASUAL_RARELY_USE_CONDOMS_PROB = "sc.casual.rarely.use.condoms.prob";
const std::string SC_CASUAL_SOMETIMES_USE_CONDOMS_PROB = "sc.casual.sometimes.use.condoms.prob";
const std::string SC_CASUAL_USUALLY_USE_CONDOMS_PROB = "sc.casual.usually.use.condoms.prob";
const std::string SC_CASUAL_ALWAYS_USE_CONDOMS_PROB = "sc.casual.always.use.condoms.prob";

const std::string EXTERNAL_INFECTION_RATE_MIN = "external.infections.per.person.day.min";
const std::string EXTERNAL_INFECTION_RATE_MAX = "external.infections.per.person.day.max";


Parameters* Parameters::instance_ = 0;

std::ostream& operator<< (std::ostream& os, const Parameters* params) {
	for (auto iter = params->props_.keys_begin(); iter != params->props_.keys_end(); ++iter) {
		os << (*iter) << " : " << params->getStringParameter(*iter) << std::endl;
	}
	return os;
}

Parameters::Parameters(repast::Properties& props) :
		props_(props)  {
}

Parameters::~Parameters() {

}

void Parameters::initialize(Properties& props) {
	if (instance_ != 0) {
		delete instance_;
	}
	instance_ = new Parameters(props);
}

Parameters* Parameters::instance() {
	if (instance_ == 0)
		throw std::domain_error("Parameters must be initialized before instance() is called");

	return instance_;
}

std::string Parameters::getStringParameter(const std::string& prop_name) const {
	std::string val = props_.getProperty(prop_name);
	if (val.length() == 0)
		throw std::invalid_argument(
				"Invalid property name '" + prop_name + "', no property found.");
	return val;
}

bool Parameters::contains(const std::string& prop_name) const {
	return props_.contains(prop_name);
}

int Parameters::getIntParameter(const std::string& prop_name) const {
	return strToInt(getStringParameter(prop_name));
}

float Parameters::getFloatParameter(const std::string& prop_name) const {
	return std::stof(getStringParameter(prop_name));
}

double Parameters::getDoubleParameter(const std::string& prop_name) const {
	return strToDouble(getStringParameter(prop_name));
}

void Parameters::getKeys(const std::string& starts_with, std::vector<std::string>& keys) {
	for (auto iter = props_.keys_begin(); iter != props_.keys_end(); ++iter) {
		// std::cout << (*iter) << std::endl;
		if (boost::starts_with(*iter, starts_with)) {
			keys.push_back(*iter);
		}
	}
}

void Parameters::putParameter(const std::string& key, bool value) {
	std::string val = value ? "true" : "false";
	props_.putProperty(key, val);
}

void Parameters::putParameter(const std::string& key, const std::string& value) {
	props_.putProperty(key, value);
}

void Parameters::putParameter(const std::string& key, double value) {
	props_.putProperty(key, value);
}

bool Parameters::getBooleanParameter(const std::string& prop_name) const {
	std::string val = getStringParameter(prop_name);
	return val == "true" || val == "TRUE";
}

} /* namespace mrsa */
