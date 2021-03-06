/*
 * PersonDataRecorder.h
 *
 *  Created on: May 12, 2016
 *      Author: nick
 */

#ifndef SRC_PERSONDATARECORDER_H_
#define SRC_PERSONDATARECORDER_H_

#include <map>

#include "StatsWriter.h"
#include "Person.h"
#include "common.h"

namespace TransModel {

enum class InfectionSource {INTERNAL, EXTERNAL, NONE};

struct PersonData {

	static const std::string header;

	int id_;
	double birth_ts, death_ts, infection_ts;
	double art_init_ts, art_stop_ts, prep_init_ts, prep_stop_ts;
	PrepStatus prep_status;
	bool infection_status, art_status, diagnosed;
	unsigned int number_of_tests;
	double time_since_last_test;
	int adherence_category;
	unsigned int adhered_interval_count, non_adhered_interval_count;
	double init_art_lag;
	unsigned int infection_source;

	PersonData(PersonPtr p, double time_of_birth);
	void writeTo(FileOutput& out);

};

class PersonDataRecorder {

	std::map<int, PersonData> data;
	StatsWriter<PersonData> writer;

private:

public:
	PersonDataRecorder(const std::string& fname, unsigned int buffer);

	virtual ~PersonDataRecorder();

	void initRecord(PersonPtr& person, double time_of_entry);
	void recordARTStart(PersonPtr& p, double ts);
	void recordARTStop(PersonPtr& p, double ts);
	void recordPREPStart(int id, double ts);
	void recordPREPStop(int id, double ts, PrepStatus status);
	void recordInfection(PersonPtr& p, double ts, InfectionSource source);
	void recordDeath(PersonPtr& p, double ts);
	void recordInitialARTLag(PersonPtr& p, double lag);
	void incrementNonAdheredIntervals(PersonPtr& p);
	void incrementAdheredIntervals(PersonPtr& p);
	void finalize(const PersonPtr& p, double ts);

};

} /* namespace TransModel */

#endif /* SRC_PERSONDATARECORDER_H_ */
