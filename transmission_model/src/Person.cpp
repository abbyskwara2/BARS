/*
 * Person.cpp
 *
 *  Created on: Oct 8, 2015
 *      Author: nick
 */

#include "Person.h"
#include "Stats.h"

using namespace Rcpp;

namespace TransModel {

Person::Person(int id, float age, bool circum_status, int steady_role, int casual_role, Diagnoser<GeometricDistribution>& diagnoser) :
		id_(id), steady_role_(steady_role), casual_role_(casual_role), age_(age), circum_status_(circum_status),
		infection_parameters_(), infectivity_(0), dead_(false), diagnosed_(false), testable_(false),
		diagnoser_(diagnoser), art_adherence_{0, AdherenceCategory::NA}, prep_adherence_{0, AdherenceCategory::NA},
		prep_status_{PrepStatus::OFF}, prep_on_at{-1} {
}

//Person::Person(int id, std::shared_ptr<RNetwork> network, double timeOfBirth) : net(network), id_(id) {
//	setAge(0);
//	setInfected(false);
//	setTimeOfBirth(timeOfBirth);
//}

Person::~Person() {
}

void Person::infect(float duration_of_infection, float time) {
	infection_parameters_.dur_inf_by_age = duration_of_infection;
	infection_parameters_.infection_status = true;
	infection_parameters_.time_since_infection = 0;
	infection_parameters_.age_at_infection = age_;
	infection_parameters_.time_of_infection = time;
}


void Person::setAge(float age) {
	age_ = age;
}

void Person::setCD4Count(float cd4_count) {
	infection_parameters_.cd4_count = cd4_count;
}

void Person::setViralLoadARTSlope(float slope) {
	infection_parameters_.vl_art_traj_slope = slope;
}

void Person::setViralLoad(float viral_load) {
	infection_parameters_.viral_load = viral_load;
}

void Person::setInfectivity(float infectivity) {
	infectivity_ = infectivity;
}

void Person::goOffART() {
	infection_parameters_.art_status = false;
}

void Person::goOnART(float time_stamp) {
	infection_parameters_.art_status = true;
	infection_parameters_.time_since_art_init = 0;
	infection_parameters_.time_of_art_init = time_stamp;
	infection_parameters_.cd4_at_art_init = infection_parameters_.cd4_count;
	infection_parameters_.vl_at_art_init = infection_parameters_.viral_load;
}

void Person::goOnPrep(double timestamp) {
	prep_status_ = PrepStatus::ON;
	prep_on_at = timestamp;
}

void Person::goOffPrep(PrepStatus off_status) {
	if (off_status == PrepStatus::ON)
		throw std::invalid_argument("goOffPrep -- status must be an off status");

	prep_status_ = off_status;
}


void Person::step(float size_of_timestep) {
	age_ += size_of_timestep / 365;
	if (infection_parameters_.infection_status) {
		++infection_parameters_.time_since_infection;
	}

	if (infection_parameters_.art_status) {
		++infection_parameters_.time_since_art_init;
	}
}

bool Person::deadOfAge(int max_age) {
	return age_ > max_age;
}

bool Person::deadOfInfection() {
	return infection_parameters_.infection_status && !infection_parameters_.art_status &&
			infection_parameters_.time_since_infection >= infection_parameters_.dur_inf_by_age;
}

bool Person::diagnose(double tick) {
	Result result = diagnoser_.test(tick, infection_parameters_);
	diagnosed_ = result == Result::POSITIVE;
	if (result != Result::NO_TEST) {
		Stats::instance()->recordTestingEvent(tick, id_, diagnosed_);
		if (diagnosed_ && prep_status_ == PrepStatus::ON) {
			prep_status_ = PrepStatus::OFF_INFECTED;
			Stats::instance()->recordPREPEvent(tick, id(), static_cast<int>(PrepStatus::OFF_INFECTED));
			Stats::instance()->personDataRecorder().recordPREPStop(this, tick, PrepStatus::OFF_INFECTED);
		}
	}
	return diagnosed_;
}

double Person::timeUntilNextTest(double tick) const {
	return diagnosed_ ? 0 : diagnoser_.timeUntilNextTest(tick);
}

} /* namespace TransModel */
