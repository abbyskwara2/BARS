/*
 * Model.cpp
 *
 *  Created on: Oct 8, 2015
 *      Author: nick
 */
#include "boost/algorithm/string.hpp"
#include "boost/filesystem.hpp"

#include "repast_hpc/Random.h"
#include "repast_hpc/RepastProcess.h"
#include "repast_hpc/Utilities.h"

#include "Parameters.h"
#include "Model.h"
#include "network_utils.h"
#include "common.h"
#include "TransmissionRunner.h"
#include "DiseaseParameters.h"
#include "PersonCreator.h"
#include "ARTScheduler.h"
#include "Stats.h"
#include "StatsBuilder.h"
#include "file_utils.h"
#include "utils.h"
#include "PrepCessationEvent.h"
#include "art_functions.h"
#include "CondomUseAssigner.h"

#include "debug_utils.h"

//#include "EventWriter.h"

using namespace Rcpp;
using namespace std;
using namespace repast;

namespace fs = boost::filesystem;

namespace TransModel {

PartnershipEvent::PEventType cod_to_PEvent(CauseOfDeath cod) {
	if (cod == CauseOfDeath::AGE) return PartnershipEvent::PEventType::ENDED_AGING_OUT;
	else if (cod == CauseOfDeath::ASM) return PartnershipEvent::PEventType::ENDED_DEATH_ASM;
	else if (cod == CauseOfDeath::INFECTION) return PartnershipEvent::PEventType::ENDED_DEATH_INFECTION;
	else {
		throw std::invalid_argument("No PEvent for specified CauseOfDeath");
	}
}

struct PersonToVALForSimulate {

	List operator()(const PersonPtr& v, int idx, double tick) const {

		return List::create(Named("na") = false, Named("vertex_names") = idx, Named("role_main") = v->steady_role(),
				Named("role_casual") = v->casual_role(), Named("inf.status") = v->isInfected(), Named("diagnosed") = v->isDiagnosed(),
				Named("age") = v->age(), Named("sqrt.age") = sqrt(v->age()));
	}
};

struct PersonToVAL {

	List operator()(const PersonPtr& p, int idx, double tick) const {
		List vertex = List::create();

		vertex["na"] = false, vertex["vertex_names"] = idx;

		vertex[C_ID] = p->id();
		vertex["age"] = p->age();
		vertex["cd4.count.today"] = p->infectionParameters().cd4_count;
		vertex["circum.status"] = p->isCircumcised();

		vertex["diagnosed"] = p->isDiagnosed();
		const Diagnoser<GeometricDistribution>& diagnoser = p->diagnoser();
		vertex["number.of.tests"] = diagnoser.testCount();
		vertex["time.until.next.test"] = diagnoser.timeUntilNextTest(tick);
		vertex["non.testers"] = !(p->isTestable());
		vertex["prep.status"] = p->isOnPrep();
		vertex["role_casual"] = p->casual_role();
		vertex["role_main"] = p->steady_role();

		if (p->isOnPrep()) {
			vertex["time.of.prep.cessation"] = p->prepParameters().stopTime();
			vertex["time.of.prep.initiation"] = p->prepParameters().startTime();
		}

		if (p->isInfected()) {
			vertex["infectivity"] = p->infectivity();
			vertex["art.status"] = p->isOnART();
			vertex["inf.status"] = p->isInfected();
			vertex["time.since.infection"] = p->infectionParameters().time_since_infection;
			vertex["time.of.infection"] = p->infectionParameters().time_of_infection;
			vertex["age.at.infection"] = p->infectionParameters().age_at_infection;
			vertex["viral.load.today"] = p->infectionParameters().viral_load;
		} else {
			vertex["infectivity"] = 0;
			vertex["art.covered"] = NA_LOGICAL;
			vertex["art.status"] = NA_LOGICAL;
			vertex["inf.status"] = false;
			vertex["time.since.infection"] = NA_REAL;
			vertex["time.of.infection"] = NA_REAL;
			vertex["age.at.infection"] = NA_REAL;
			vertex["viral.load.today"] = 0;
		}

		vertex["adherence.category"] = static_cast<int>(p->adherence().category);

		if (p->isOnART()) {
			vertex["time.since.art.initiation"] = p->infectionParameters().time_since_art_init;
			vertex["time.of.art.initiation"] = p->infectionParameters().time_of_art_init;
			vertex["vl.art.traj.slope"] = p->infectionParameters().vl_art_traj_slope;
			vertex["cd4.at.art.initiation"] = p->infectionParameters().cd4_at_art_init;
			vertex["vl.at.art.initiation"] = p->infectionParameters().vl_at_art_init;
		} else {
			vertex["time.since.art.initiation"] = NA_REAL;
			vertex["time.of.art.initiation"] = NA_REAL;
			vertex["vl.art.traj.slope"] = NA_REAL;
			vertex["cd4.at.art.initiation"] = NA_REAL;
			vertex["vl.at.art.initiation"] = NA_REAL;
		}

		return vertex;
	}
};

shared_ptr<TransmissionRunner> create_transmission_runner() {
	float circ_mult = (float) Parameters::instance()->getDoubleParameter(CIRCUM_MULT);
	float prep_mult = (float) Parameters::instance()->getDoubleParameter(PREP_TRANS_REDUCTION);
	float condom_mult = (float) Parameters::instance()->getDoubleParameter(INFECTIVITY_REDUCTION_CONDOM);
	float infective_insertive_mult = (float) Parameters::instance()->getDoubleParameter(INFECTIVE_INSERTIVE_MULT);

//	string str_dur_inf = Parameters::instance()->getStringParameter(DUR_INF_BY_AGE);
//	vector<string> tokens;
//	// TODO error checking for correct number of values
//	repast::tokenize(str_dur_inf, tokens, ",");
//	for (auto s : tokens) {
//		dur_inf_by_age.push_back((float) std::atof(s.c_str()));
//	}
	// not by age yet
	float duration = Parameters::instance()->getFloatParameter(DUR_INF_BY_AGE);
	vector<float> dur_inf_by_age(4, duration);
	return make_shared<TransmissionRunner>(circ_mult, prep_mult, condom_mult, infective_insertive_mult, dur_inf_by_age);
}

CD4Calculator create_CD4Calculator() {
	// float size_of_timestep, float cd4_recovery_time,
	// float cd4_at_infection_male, float per_day_cd4_recovery
	float size_of_timestep = (float) Parameters::instance()->getDoubleParameter(SIZE_OF_TIMESTEP);
	float cd4_recovery_time = (float) Parameters::instance()->getDoubleParameter(CD4_RECOVERY_TIME);
	float cd4_at_infection = (float) Parameters::instance()->getDoubleParameter(CD4_AT_INFECTION_MALE);
	float per_day_cd4_recovery = (float) Parameters::instance()->getDoubleParameter(PER_DAY_CD4_RECOVERY);

	BValues b_values;
	b_values.b1_ref = Parameters::instance()->getFloatParameter(B1_REF);
	b_values.b2_african = Parameters::instance()->getFloatParameter(B2_AFRICAN);
	b_values.b3_female = Parameters::instance()->getFloatParameter(B3_FEMALE);
	b_values.b4_cd4_ref = Parameters::instance()->getFloatParameter(B4_CD4_REF);
	b_values.b5_african = Parameters::instance()->getFloatParameter(B5_AFRICAN);
	b_values.b6_age_15to29 = Parameters::instance()->getFloatParameter(B6_AGE_15TO29);
	b_values.b6_age_30to39 = Parameters::instance()->getFloatParameter(B6_AGE_30TO39);
	b_values.b6_age_40to49 = Parameters::instance()->getFloatParameter(B6_AGE_40TO49);
	b_values.b6_age_50ormore = Parameters::instance()->getFloatParameter(B6_AGE_50ORMORE);
	return CD4Calculator(size_of_timestep, cd4_recovery_time, cd4_at_infection, per_day_cd4_recovery, b_values);
}

ViralLoadCalculator create_ViralLoadCalculator() {
	SharedViralLoadParameters params;
	params.time_infection_to_peak_load = (float) Parameters::instance()->getDoubleParameter(
			TIME_INFECTION_TO_PEAK_VIRAL_LOAD);
	params.time_infection_to_set_point = (float) Parameters::instance()->getDoubleParameter(
			TIME_INFECTION_TO_VIRAL_SET_POINT);
	params.time_infection_to_late_stage = (float) Parameters::instance()->getDoubleParameter(
			TIME_INFECTION_TO_LATE_STAGE);
	params.time_to_full_supp = (float) Parameters::instance()->getDoubleParameter(TIME_TO_FULL_SUPP);
	params.peak_viral_load = (float) Parameters::instance()->getDoubleParameter(PEAK_VIRAL_LOAD);
	params.set_point_viral_load = (float) Parameters::instance()->getDoubleParameter(SET_POINT_VIRAL_LOAD);
	params.late_stage_viral_load = (float) Parameters::instance()->getDoubleParameter(LATE_STAGE_VIRAL_LOAD);
	params.undetectable_viral_load = (float) Parameters::instance()->getDoubleParameter(UNDETECTABLE_VL);

	return ViralLoadCalculator(params);
}

ViralLoadSlopeCalculator create_ViralLoadSlopeCalculator() {
	float time_to_full_supp = Parameters::instance()->getFloatParameter(TIME_TO_FULL_SUPP);
	float undetectable_vl = Parameters::instance()->getFloatParameter(UNDETECTABLE_VL);
	return ViralLoadSlopeCalculator(undetectable_vl, time_to_full_supp);
}

void init_stage_map(map<float, shared_ptr<Stage>> &stage_map) {
	float acute_max = (float) Parameters::instance()->getDoubleParameter(ACUTE_LENGTH_MAX);
	float chronic_max = (float) Parameters::instance()->getDoubleParameter(CHRONIC_LENGTH_MAX);
	//float late_max = (float) Parameters::instance()->getDoubleParameter(LATE_LENGTH_MAX);
	float acute_mult = (float) Parameters::instance()->getDoubleParameter(ACUTE_MULT);
	float late_mult = (float) Parameters::instance()->getDoubleParameter(LATE_MULT);
	float baseline_infectivity = (float) Parameters::instance()->getDoubleParameter(MIN_CHRONIC_INFECTIVITY_UNADJ);
	float viral_load_incr = (float) Parameters::instance()->getDoubleParameter(VIRAL_LOAD_LOG_INCREMENT);

	stage_map.emplace(acute_max, make_shared<AcuteStage>(baseline_infectivity, acute_mult, Range<float>(1, acute_max), viral_load_incr));
	stage_map.emplace(chronic_max,
			make_shared<ChronicStage>(baseline_infectivity, Range<float>(acute_max, chronic_max), viral_load_incr));
	// make late_max essentially open ended as infected persons
	// on ART can be in late stage forever where stage is not necessarily
	// the medical stage, but the Stage class for our purposes.
	float late_max = std::numeric_limits<float>::max();
	stage_map.emplace(late_max,
			make_shared<LateStage>(baseline_infectivity, late_mult, Range<float>(chronic_max, late_max), viral_load_incr));
}

void init_generators() {
	float non_tester_rate = Parameters::instance()->getDoubleParameter(NON_TESTERS_PROP);
	BinomialGen coverage(repast::Random::instance()->engine(),
			boost::random::binomial_distribution<>(1, non_tester_rate));
	Random::instance()->putGenerator(NON_TESTERS_BINOMIAL, new DefaultNumberGenerator<BinomialGen>(coverage));

	float circum_rate = Parameters::instance()->getDoubleParameter(CIRCUM_RATE);
	BinomialGen rate(repast::Random::instance()->engine(), boost::random::binomial_distribution<>(1, circum_rate));
	Random::instance()->putGenerator(CIRCUM_STATUS_BINOMIAL, new DefaultNumberGenerator<BinomialGen>(rate));
}

void init_stats() {
	StatsBuilder builder(output_directory(Parameters::instance()));
	builder.countsWriter(Parameters::instance()->getStringParameter(COUNTS_PER_TIMESTEP_OUTPUT_FILE));
	builder.partnershipEventWriter(Parameters::instance()->getStringParameter(PARTNERSHIP_EVENTS_FILE));
	builder.infectionEventWriter(Parameters::instance()->getStringParameter(INFECTION_EVENTS_FILE));
	builder.biomarkerWriter(Parameters::instance()->getStringParameter(BIOMARKER_FILE));
	builder.deathEventWriter(Parameters::instance()->getStringParameter(DEATH_EVENT_FILE));
	builder.personDataRecorder(Parameters::instance()->getStringParameter(PERSON_DATA_FILE));
	builder.testingEventWriter(Parameters::instance()->getStringParameter(TESTING_EVENT_FILE));
	builder.artEventWriter(Parameters::instance()->getStringParameter(ART_EVENT_FILE));
	builder.prepEventWriter(Parameters::instance()->getStringParameter(PREP_EVENT_FILE));

	builder.createStatsSingleton();
}

void init_network_save(Model* model) {
	string save_prop = Parameters::instance()->getStringParameter(NET_SAVE_AT);
	vector<string> ats;
	boost::split(ats, save_prop, boost::is_any_of(","));
	if (ats.size() > 0) {
		ScheduleRunner& runner = RepastProcess::instance()->getScheduleRunner();
		for (auto& at : ats) {
			boost::trim(at);
			if (at == "end") {
				//std::cout << "scheduling at end" << std::endl;
				runner.scheduleEndEvent(Schedule::FunctorPtr(new MethodFunctor<Model>(model, &Model::saveRNetwork)));
			} else {
				double tick = stod(at);
				//std::cout << "scheduling at " << tick << std::endl;
				runner.scheduleEvent(tick + 0.1,
						Schedule::FunctorPtr(new MethodFunctor<Model>(model, &Model::saveRNetwork)));
			}
		}
	}
}

void init_biomarker_logging(Network<Person>& net, std::set<int>& ids_to_log) {
	int number_to_log = Parameters::instance()->getIntParameter(BIOMARKER_LOG_COUNT);
	std::vector<PersonPtr> persons;
	for (auto iter = net.verticesBegin(); iter != net.verticesEnd(); ++iter) {
		persons.push_back(*iter);
	}

	IntUniformGenerator gen = Random::instance()->createUniIntGenerator(0, persons.size() - 1);
	for (int i = 0; i < number_to_log; ++i) {
		int id = (int) gen.next();
		while (ids_to_log.find(id) != ids_to_log.end()) {
			id = (int) gen.next();
		}
		ids_to_log.emplace(id);
	}
}

void init_trans_params(TransmissionParameters& params) {
	double size_of_time_step = Parameters::instance()->getDoubleParameter(SIZE_OF_TIMESTEP);
	params.prop_steady_sex_acts = Parameters::instance()->getDoubleParameter(PROP_STEADY_SEX_ACTS) * size_of_time_step;
	params.prop_casual_sex_acts = Parameters::instance()->getDoubleParameter(PROP_CASUAL_SEX_ACTS) * size_of_time_step;
}

std::shared_ptr<DayRangeCalculator> create_art_lag_calc() {
	DayRangeCalculatorCreator creator;
	vector<string> lag_keys;
	Parameters::instance()->getKeys(ART_LAG_PREFIX, lag_keys);
	for (auto& key : lag_keys) {
		creator.addBin(Parameters::instance()->getStringParameter(key));
	}
	return creator.createCalculator();
}

std::shared_ptr<GeometricDistribution> create_cessation_generator() {
	double prob = Parameters::instance()->getDoubleParameter(PREP_DAILY_STOP_PROB);
	// 1.1 so at least a day on prep, and .1 so after step loop.
	return std::make_shared<GeometricDistribution>(prob, 1.1);
}

void add_condom_use_prob(CondomUseAssignerFactory& factory, PartnershipType ptype, int network_type,
		const std::string& category_param, const std::string& use_param) {
	double cat_prob = Parameters::instance()->getDoubleParameter(category_param);
	double use_prob = Parameters::instance()->getDoubleParameter(use_param);
	factory.addProbability(ptype, network_type, cat_prob, use_prob);
}

CondomUseAssigner create_condom_use_assigner() {
	CondomUseAssignerFactory factory;
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, STEADY_NETWORK_TYPE, SD_STEADY_NEVER_USE_CONDOMS,
			SD_STEADY_NEVER_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, STEADY_NETWORK_TYPE, SD_STEADY_RARELY_USE_CONDOMS,
			SD_STEADY_RARELY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, STEADY_NETWORK_TYPE, SD_STEADY_SOMETIMES_USE_CONDOMS,
			SD_STEADY_SOMETIMES_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, STEADY_NETWORK_TYPE, SD_STEADY_USUALLY_USE_CONDOMS,
			SD_STEADY_USUALLY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, STEADY_NETWORK_TYPE, SD_STEADY_ALWAYS_USE_CONDOMS,
			SD_STEADY_ALWAYS_USE_CONDOMS_PROB);

	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, CASUAL_NETWORK_TYPE, SD_CASUAL_NEVER_USE_CONDOMS,
			SD_CASUAL_NEVER_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, CASUAL_NETWORK_TYPE, SD_CASUAL_RARELY_USE_CONDOMS,
			SD_CASUAL_RARELY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, CASUAL_NETWORK_TYPE, SD_CASUAL_SOMETIMES_USE_CONDOMS,
			SD_CASUAL_SOMETIMES_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, CASUAL_NETWORK_TYPE, SD_CASUAL_USUALLY_USE_CONDOMS,
			SD_CASUAL_USUALLY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SERODISCORDANT, CASUAL_NETWORK_TYPE, SD_CASUAL_ALWAYS_USE_CONDOMS,
			SD_CASUAL_ALWAYS_USE_CONDOMS_PROB);

	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, STEADY_NETWORK_TYPE, SC_STEADY_NEVER_USE_CONDOMS,
			SC_STEADY_NEVER_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, STEADY_NETWORK_TYPE, SC_STEADY_RARELY_USE_CONDOMS,
			SC_STEADY_RARELY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, STEADY_NETWORK_TYPE, SC_STEADY_SOMETIMES_USE_CONDOMS,
			SC_STEADY_SOMETIMES_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, STEADY_NETWORK_TYPE, SC_STEADY_USUALLY_USE_CONDOMS,
			SC_STEADY_USUALLY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, STEADY_NETWORK_TYPE, SC_STEADY_ALWAYS_USE_CONDOMS,
			SC_STEADY_ALWAYS_USE_CONDOMS_PROB);

	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, CASUAL_NETWORK_TYPE, SC_CASUAL_NEVER_USE_CONDOMS,
			SC_CASUAL_NEVER_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, CASUAL_NETWORK_TYPE, SC_CASUAL_RARELY_USE_CONDOMS,
			SC_CASUAL_RARELY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, CASUAL_NETWORK_TYPE, SC_CASUAL_SOMETIMES_USE_CONDOMS,
			SC_CASUAL_SOMETIMES_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, CASUAL_NETWORK_TYPE, SC_CASUAL_USUALLY_USE_CONDOMS,
			SC_CASUAL_USUALLY_USE_CONDOMS_PROB);
	add_condom_use_prob(factory, PartnershipType::SEROCONCORDANT, CASUAL_NETWORK_TYPE, SC_CASUAL_ALWAYS_USE_CONDOMS,
			SC_CASUAL_ALWAYS_USE_CONDOMS_PROB);

	return factory.createAssigner();
}

RangeWithProbability create_ASM_runner() {
	RangeWithProbabilityCreator creator;
	vector<string> keys;
	Parameters::instance()->getKeys(ASM_PREFIX, keys);
	for (auto& key : keys) {
		creator.addBin(key, Parameters::instance()->getDoubleParameter(key));
	}
	return creator.createRangeWithProbability();
}

Model::Model(shared_ptr<RInside>& ri, const std::string& net_var, const std::string& cas_net_var) :
		R(ri), net(false), trans_runner(create_transmission_runner()), cd4_calculator(create_CD4Calculator()), viral_load_calculator(
				create_ViralLoadCalculator()), viral_load_slope_calculator(create_ViralLoadSlopeCalculator()), current_pop_size {
				0 }, previous_pop_size { 0 }, stage_map { }, persons_to_log { }, person_creator { trans_runner,
				Parameters::instance()->getDoubleParameter(DAILY_TESTING_PROB),
				Parameters::instance()->getDoubleParameter(DETECTION_WINDOW) }, trans_params { }, art_lag_calculator {
				create_art_lag_calc() }, cessation_generator { create_cessation_generator() }, condom_assigner {
				create_condom_use_assigner() }, asm_runner { create_ASM_runner() } {

	// get initial stats
	init_stats();
	init_trans_params(trans_params);

	List rnet = as<List>((*R)[net_var]);
	initialize_network(rnet, net, person_creator, condom_assigner, STEADY_NETWORK_TYPE);
	rnet = as<List>((*R)[cas_net_var]);
	initialize_edges(rnet, net, condom_assigner, CASUAL_NETWORK_TYPE);

	init_stage_map(stage_map);
	init_network_save(this);

	current_pop_size = net.vertexCount();

	init_biomarker_logging(net, persons_to_log);
	Stats* stats = TransModel::Stats::instance();
	stats->currentCounts().main_edge_count = net.edgeCount(STEADY_NETWORK_TYPE);
	stats->currentCounts().casual_edge_count = net.edgeCount(CASUAL_NETWORK_TYPE);
	stats->currentCounts().size = net.vertexCount();
	for (auto iter = net.verticesBegin(); iter != net.verticesEnd(); ++iter) {
		PersonPtr p = *iter;
		stats->personDataRecorder().initRecord(p, 0);
		if (p->isInfected()) {
			++stats->currentCounts().internal_infected;
			stats->personDataRecorder().recordInfection(p, p->infectionParameters().time_of_infection, InfectionSource::INTERNAL);
		}
	}
	stats->resetForNextTimeStep();

	init_generators();

	ScheduleRunner& runner = RepastProcess::instance()->getScheduleRunner();
	runner.scheduleStop(Parameters::instance()->getDoubleParameter("stop.at"));
	runner.scheduleEvent(1, 1, Schedule::FunctorPtr(new MethodFunctor<Model>(this, &Model::step)));
	runner.scheduleEndEvent(Schedule::FunctorPtr(new MethodFunctor<Model>(this, &Model::atEnd)));

	initPrepCessation();

	//write_edges(net, "./edges_at_1.csv");
}

void Model::initPrepCessation() {
	ScheduleRunner& runner = RepastProcess::instance()->getScheduleRunner();
	for (auto iter = net.verticesBegin(); iter != net.verticesEnd(); ++iter) {
		PersonPtr person = *iter;
		if (person->isOnPrep()) {
			double stop_time = person->prepParameters().stopTime();
			runner.scheduleEvent(stop_time, Schedule::FunctorPtr(new PrepCessationEvent(person, stop_time)));
			double start_time = person->prepParameters().startTime();
			Stats::instance()->recordPREPEvent(start_time, person->id(), static_cast<int>(PrepStatus::ON));
			Stats::instance()->personDataRecorder().recordPREPStart(person->id(), start_time);
		}
	}
}

void Model::atEnd() {
	double ts = RepastProcess::instance()->getScheduleRunner().currentTick();
	PersonDataRecorder& pdr = Stats::instance()->personDataRecorder();
	for (auto iter = net.verticesBegin(); iter != net.verticesEnd(); ++iter) {
		pdr.finalize(*iter, ts);
	}

	// forces stat writing via destructors
	delete Stats::instance();

	//write_edges(net, "./edges_at_end.csv");
}

Model::~Model() {
}

void Model::updateThetaForm(const std::string& var_name) {
	NumericVector theta_form = as<NumericVector>((*R)[var_name]);
	theta_form[0] = theta_form[0] + std::log(previous_pop_size) - std::log(current_pop_size);
	((*R)[var_name]) = theta_form;
}

void Model::countOverlap() {
	int target_type = 0;
	int other_type = 0;
	if (net.edgeCount(STEADY_NETWORK_TYPE) < net.edgeCount(CASUAL_NETWORK_TYPE)) {
		target_type = STEADY_NETWORK_TYPE;
		other_type = CASUAL_NETWORK_TYPE;
	} else {
		target_type = CASUAL_NETWORK_TYPE;
		other_type = STEADY_NETWORK_TYPE;
	}

	Stats* stats = Stats::instance();
	for (auto iter = net.edgesBegin(); iter != net.edgesEnd(); ++iter) {
		if ((*iter)->type() == target_type) {
			EdgePtr<Person> edge = (*iter);
			if (net.hasEdge(edge->v1(), edge->v2(), other_type) || net.hasEdge(edge->v2(), edge->v1(), other_type)) {
				++(stats->currentCounts().overlaps);
			}
		}
	}
}

void Model::step() {
	double t = RepastProcess::instance()->getScheduleRunner().currentTick();
	Stats* stats = Stats::instance();
	stats->currentCounts().tick = t;

	PersonToVALForSimulate p2val;
	float max_survival = Parameters::instance()->getFloatParameter(MAX_AGE);
	float size_of_timestep = Parameters::instance()->getIntParameter(SIZE_OF_TIMESTEP);

	if ((int) t % 100 == 0)
		std::cout << " ---- " << t << " ---- " << std::endl;
	simulate(R, net, p2val, condom_assigner, t);
	if (Parameters::instance()->getBooleanParameter(COUNT_OVERLAPS)) {
		countOverlap();
	} else {
		stats->currentCounts().overlaps = -1;
	}
	entries(t, size_of_timestep);
	runTransmission(t);
	vector<PersonPtr> uninfected;
	updateVitals(t, size_of_timestep, max_survival, uninfected);
	runExternalInfections(uninfected, t);
	previous_pop_size = current_pop_size;
	current_pop_size = net.vertexCount();

	//std::cout << "pop sizes: " << previous_pop_size << ", " << current_pop_size << std::endl;
	updateThetaForm("theta.form");
	updateThetaForm("theta.form_cas");

	stats->currentCounts().main_edge_count = net.edgeCount(STEADY_NETWORK_TYPE);
	stats->currentCounts().casual_edge_count = net.edgeCount(CASUAL_NETWORK_TYPE);
	stats->currentCounts().size = net.vertexCount();
	stats->resetForNextTimeStep();
}

void Model::schedulePostDiagnosisART(PersonPtr person, std::map<double, ARTScheduler*>& art_map, double tick,
		float size_of_timestep) {
	double lag = art_lag_calculator->calculateLag(size_of_timestep);
	Stats::instance()->personDataRecorder().recordInitialARTLag(person, lag);

	double art_at_tick = lag + tick;
	ARTScheduler* scheduler = nullptr;
	auto iter = art_map.find(art_at_tick);
	if (iter == art_map.end()) {
		scheduler = new ARTScheduler((float) art_at_tick);
		RepastProcess::instance()->getScheduleRunner().scheduleEvent(art_at_tick - 0.1,
				repast::Schedule::FunctorPtr(scheduler));
		art_map.emplace(art_at_tick, scheduler);
	} else {
		scheduler = iter->second;
	}
	scheduler->addPerson(person);
}

// ASSUMES PERSON IS UNINFECTED
void Model::updatePREPUse(double tick, double prob, PersonPtr person) {
	if (!person->isOnPrep() && Random::instance()->nextDouble() <= prob) {
		ScheduleRunner& runner = RepastProcess::instance()->getScheduleRunner();
		double stop_time = tick + cessation_generator->next();
		person->goOnPrep(tick, stop_time);
		Stats::instance()->recordPREPEvent(tick, person->id(), static_cast<int>(PrepStatus::ON));
		Stats::instance()->personDataRecorder().recordPREPStart(person->id(), tick);
		runner.scheduleEvent(stop_time, Schedule::FunctorPtr(new PrepCessationEvent(person, stop_time)));
	}
}

void Model::updateVitals(double t, float size_of_timestep, int max_age, vector<PersonPtr>& uninfected) {
	unsigned int dead_count = 0;
	Stats* stats = Stats::instance();
	map<double, ARTScheduler*> art_map;

	double p = Parameters::instance()->getDoubleParameter(PREP_DAILY_STOP_PROB);
	double k = Parameters::instance()->getDoubleParameter(PREP_USE_PROP);
	double on_prep_prob = (p * k) / (1 - k);

	uninfected.reserve(net.vertexCount());

	for (auto iter = net.verticesBegin(); iter != net.verticesEnd();) {
		PersonPtr person = (*iter);
		// update viral load
		if (person->isInfected()) {
			if (person->isOnART()) {
				float slope = viral_load_slope_calculator.calculateSlope(person->infectionParameters());
				person->setViralLoadARTSlope(slope);
			}

			float viral_load = viral_load_calculator.calculateViralLoad(person->infectionParameters());
			person->setViralLoad(viral_load);
			// update cd4
			float cd4 = cd4_calculator.calculateCD4(person->age(), person->infectionParameters());
			person->setCD4Count(cd4);

			// select stage, and use it
			float infectivity = stage_map.upper_bound(person->timeSinceInfection())->second->calculateInfectivity(
					person->infectionParameters());
			person->setInfectivity(infectivity);
		} else {
			updatePREPUse(t, on_prep_prob, person);
		}

		if (persons_to_log.find(person->id()) != persons_to_log.end()) {
			stats->recordBiomarker(t, person);
		}

		if (person->isTestable() && !person->isDiagnosed()) {
			if (person->diagnose(t)) {
				schedulePostDiagnosisART(person, art_map, t, size_of_timestep);
			}
		}

		person->step(size_of_timestep);
		CauseOfDeath cod = dead(t, person, max_age);
		if (cod != CauseOfDeath::NONE) {
			vector<EdgePtr<Person>> edges;
			PartnershipEvent::PEventType pevent_type = cod_to_PEvent(cod);
			net.getEdges(person, edges);
			for (auto edge : edges) {
				//cout << edge->id() << "," << static_cast<int>(cod) << "," << static_cast<int>(pevent_type) << endl;
				Stats::instance()->recordPartnershipEvent(t, edge->id(), edge->v1()->id(), edge->v2()->id(), pevent_type, edge->type());
			}
			iter = net.removeVertex(iter);
			++dead_count;
		} else {
			// don't count dead uninfected persons
			if (!person->isInfected()) {
				++stats->currentCounts().uninfected;
				uninfected.push_back(person);
			}
			++iter;
		}
	}
}

void Model::runExternalInfections(vector<PersonPtr>& uninfected, double t) {
	double min = Parameters::instance()->getDoubleParameter(EXTERNAL_INFECTION_RATE_MIN);
	double max = Parameters::instance()->getDoubleParameter(EXTERNAL_INFECTION_RATE_MAX);
	double val = Random::instance()->createUniDoubleGenerator(min, max).next();
	// std::cout << val << std::endl;
	double prob = uninfected.size() * val;
	//std::cout << uninfected.size() << ", " << prob << std::endl;
	if (Random::instance()->nextDouble() <= prob) {
		Stats* stats = Stats::instance();
		PersonPtr p = uninfected[(int)Random::instance()->createUniIntGenerator(0, uninfected.size() - 1).next()];
		infectPerson(p, t);
		++stats->currentCounts().external_infected;
		stats->personDataRecorder().recordInfection(p, t, InfectionSource::EXTERNAL);
	}
}

void Model::infectPerson(PersonPtr& person, double time_stamp) {
	trans_runner->infect(person, time_stamp);

	vector<EdgePtr<Person>> edges;
	net.getEdges(person, edges);
	for (EdgePtr<Person> ptr : edges) {
		condom_assigner.initEdge(ptr);
	}
}

void Model::entries(double tick, float size_of_timestep) {
	float min_age = Parameters::instance()->getFloatParameter(MIN_AGE);
	size_t pop_size = net.vertexCount();
	if (pop_size > 0) {
		double births_prob = Parameters::instance()->getDoubleParameter(DAILY_ENTRY_RATE);
		PoissonGen birth_gen(Random::instance()->engine(),
				boost::random::poisson_distribution<>(births_prob));
		DefaultNumberGenerator<PoissonGen> gen(birth_gen);
		int entries = (int) gen.next();
		Stats* stats = Stats::instance();
		stats->currentCounts().entries = entries;
		//std::cout << "entries: " << entries << std::endl;

		double infected_prob = Parameters::instance()->getDoubleParameter(INIT_HIV_PREV_ENTRIES);

		for (int i = 0; i < entries; ++i) {
			VertexPtr<Person> p = person_creator(tick, min_age);
			if (Random::instance()->nextDouble() <= infected_prob) {
				// as if infected at previous timestep
				float infected_at = tick - (size_of_timestep * 1);
				trans_runner->infect(p, infected_at);
				float viral_load = viral_load_calculator.calculateViralLoad(p->infectionParameters());
				p->setViralLoad(viral_load);
				// update cd4
				float cd4 = cd4_calculator.calculateCD4(p->age(), p->infectionParameters());
				p->setCD4Count(cd4);
				++stats->currentCounts().infected_at_entry;
				stats->recordInfectionEvent(infected_at, p);
			}
			net.addVertex(p);
			Stats::instance()->personDataRecorder().initRecord(p, tick);
		}
	}
}

std::string get_net_out_filename(const std::string& file_name) {
	long tick = floor(RepastProcess::instance()->getScheduleRunner().currentTick());
	fs::path filepath(file_name);
	std::string stem = filepath.stem().string();

	std::stringstream ss;
	ss << stem << "_" << tick << filepath.extension().string();
	fs::path newName(filepath.parent_path() / ss.str());

	return newName.string();
}

void Model::saveRNetwork() {
	List rnet;
	std::map<unsigned int, unsigned int> idx_map;
	PersonToVAL p2val;

	long tick = floor(RepastProcess::instance()->getScheduleRunner().currentTick());
	create_r_network(tick, rnet, net, idx_map, p2val, STEADY_NETWORK_TYPE);
	std::string file_name = output_directory(Parameters::instance()) + "/"
			+ Parameters::instance()->getStringParameter(NET_SAVE_FILE);
	as<Function>((*R)["nw_save"])(rnet, unique_file_name(get_net_out_filename(file_name)), tick);

	if (Parameters::instance()->contains(CASUAL_NET_SAVE_FILE)) {
		idx_map.clear();
		List cas_net;
		create_r_network(tick, cas_net, net, idx_map, p2val, CASUAL_NETWORK_TYPE);
		file_name = output_directory(Parameters::instance()) + "/"
				+ Parameters::instance()->getStringParameter(CASUAL_NET_SAVE_FILE);
		as<Function>((*R)["nw_save"])(cas_net, unique_file_name(get_net_out_filename(file_name)), tick);
	}
}

CauseOfDeath Model::dead(double tick, PersonPtr person, int max_age) {
	int death_count = 0;
	CauseOfDeath cod = CauseOfDeath::NONE;
	// dead of old age
	if (person->deadOfAge(max_age)) {
		++death_count;
		++Stats::instance()->currentCounts().age_deaths;
		Stats::instance()->recordDeathEvent(tick, person, DeathEvent::AGE);
		Stats::instance()->personDataRecorder().recordDeath(person, tick);
		cod = CauseOfDeath::AGE;
	}

	if (cod == CauseOfDeath::NONE && person->deadOfInfection()) {
		// infection deaths
		++death_count;
		++Stats::instance()->currentCounts().infection_deaths;
		Stats::instance()->recordDeathEvent(tick, person, DeathEvent::INFECTION);
		Stats::instance()->personDataRecorder().recordDeath(person, tick);
		cod = CauseOfDeath::INFECTION;
	}

	if (cod == CauseOfDeath::NONE && asm_runner.run(person->age(), Random::instance()->nextDouble())) {
		// asm deaths
		++death_count;
		++Stats::instance()->currentCounts().asm_deaths;
		Stats::instance()->recordDeathEvent(tick, person, DeathEvent::ASM);
		Stats::instance()->personDataRecorder().recordDeath(person, tick);
		cod = CauseOfDeath::ASM;
	}

	person->setDead(cod != CauseOfDeath::NONE);
	return cod;
}

bool Model::hasSex(int edge_type) {
	double prob;
	if (edge_type == STEADY_NETWORK_TYPE) {
		prob = trans_params.prop_steady_sex_acts;
	} else {
		prob = trans_params.prop_casual_sex_acts;
	}

	return Random::instance()->nextDouble() <= prob;
}

void record_sex_act(int edge_type, bool condom_used, bool discordant, Stats* stats) {
	++stats->currentCounts().sex_acts;
	if (edge_type == STEADY_NETWORK_TYPE) {

		++stats->currentCounts().steady_sex_acts;
		if (condom_used) {
			if (discordant) {
				++stats->currentCounts().sd_steady_sex_with_condom;
			} else {
				++stats->currentCounts().sc_steady_sex_with_condom;
			}
		} else {
			if (discordant) {
				++stats->currentCounts().sd_steady_sex_without_condom;
			} else {
				++stats->currentCounts().sc_steady_sex_without_condom;
			}
		}
	} else {
		++stats->currentCounts().casual_sex_acts;
		if (condom_used) {
			if (discordant) {
				++stats->currentCounts().sd_casual_sex_with_condom;
			} else {
				++stats->currentCounts().sc_casual_sex_with_condom;
			}
		} else {
			if (discordant) {
				++stats->currentCounts().sd_casual_sex_without_condom;
			} else {
				++stats->currentCounts().sc_casual_sex_without_condom;
			}
		}
	}
}

void Model::runTransmission(double time_stamp) {
	vector<PersonPtr> infecteds;

	//std::cout << sex_acts_per_time_step << ", " << node_count << ", " << edge_count << ", " << prob << std::endl;
	Stats* stats = Stats::instance();
	for (auto iter = net.edgesBegin(); iter != net.edgesEnd(); ++iter) {
		int type = (*iter)->type();
		if (hasSex(type)) {
			bool condom_used = (*iter)->useCondom(Random::instance()->nextDouble());
			bool discordant = false;
			PersonPtr out_p = (*iter)->v1();
			PersonPtr in_p = (*iter)->v2();
			if (out_p->isInfected() && !in_p->isInfected()) {
				discordant = true;

				if (trans_runner->determineInfection(out_p, in_p, condom_used, type)) {
					infecteds.push_back(in_p);
					Stats::instance()->recordInfectionEvent(time_stamp, out_p, in_p, false, (*iter)->type());
				}
			} else if (!out_p->isInfected() && in_p->isInfected()) {
				discordant = true;

				if (trans_runner->determineInfection(in_p, out_p, condom_used, type)) {
					infecteds.push_back(out_p);
					Stats::instance()->recordInfectionEvent(time_stamp, in_p, out_p, false, (*iter)->type());
				}
			}

			record_sex_act(type, condom_used, discordant, stats);
		}
	}

	for (auto& person : infecteds) {
		// if person has multiple partners who are infected,
		// person gets multiple chances to become infected from them
		// and so may appear more than once in the infecteds list
		if (!person->isInfected()) {
			infectPerson(person, time_stamp);
			++stats->currentCounts().internal_infected;
			stats->personDataRecorder().recordInfection(person, time_stamp, InfectionSource::INTERNAL);
		}
	}
}

} /* namespace TransModel */
