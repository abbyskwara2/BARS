/*
 * utils.cpp
 *
 *  Created on: Feb 16, 2016
 *      Author: nick
 */
#include <exception>
#include <map>

#include "utils.h"

#include "boost/tokenizer.hpp"
#include "boost/algorithm/string.hpp"

//#include "boost/algorithm/string.hpp"

using namespace std;

namespace TransModel {

const std::string ACUTE_LENGTH = "acute.length";
const std::string CHRONIC_LENGTH = "chronic.length";
const std::string LATE_LENGTH = "late.length";

void parse_parameters(std::map<string, double>& props, const std::string& param_string) {

	boost::char_separator<char> comma_sep(",");
	boost::tokenizer<boost::char_separator<char> > comma_tok(param_string, comma_sep);

	for (auto item : comma_tok) {
		boost::trim(item);
		size_t pos = item.find_first_of("=");
		if (pos == std::string::npos) {
			throw invalid_argument("Invalid parameter: " + item);
		}

		string key(item.substr(0, pos));
		boost::trim(key);
		if (key.length() == 0) {
			throw invalid_argument("Invalid parameter: " + item);
		}

		string val(item.substr(pos + 1, item.length()));
		boost::trim(val);
		if (val.length() == 0) {
			throw invalid_argument("Invalid parameter: " + item);
		}
		props.emplace(key, stod(val));
	}
}

void param_string_to_R_vars(const std::string& param_string, std::shared_ptr<RInside> R) {
	map<string, double> props;
	parse_parameters(props, param_string);
	for (auto item : props) {
		(*R)[item.first] = item.second;
	}
}

void init_parameters(const std::string& non_derived, const std::string& derived, const std::string& param_string, Parameters* params, std::shared_ptr<RInside> R) {

	std::string cmd = "source(file=\"" + non_derived + "\")";
	R->parseEvalQ(cmd);

	param_string_to_R_vars(param_string, R);

	cmd = "source(file=\"" + derived + "\")";
	R->parseEvalQ(cmd);

	SEXP result;
	R->parseEval("ls()", result);

	Rcpp::List list = Rcpp::as<Rcpp::List>(result);
	for (auto& v : list) {
		std::string name(Rcpp::as<std::string>(v));
		//std::string upper_name(Rcpp::as<std::string>(v));
		//boost::to_upper(upper_name);
		//boost::replace_all(upper_name, ".", "_");
		SEXP val((*R)[name]);
		//std::cout << name << ": " << TYPEOF(val) <<  std::endl;
		if (TYPEOF(val) == REALSXP) {
			Rcpp::NumericVector v = Rcpp::as<Rcpp::NumericVector>(val);
			if (v.size() == 1) {
				params->putParameter(name, v[0]);
			}
			//std::cout << "extern const std::string " << upper_name << ";" << std::endl; //" = \"" << name << "\";" << std::endl;
		} else if (TYPEOF(val) == INTSXP && (name == ACUTE_LENGTH || name == CHRONIC_LENGTH ||
				name == LATE_LENGTH)) {
			Rcpp::IntegerVector vec = Rcpp::as<Rcpp::IntegerVector>(val);
			params->putParameter(name + ".min", (double)vec[0]);
			params->putParameter(name + ".max", (double)vec[vec.size() - 1]);
			//std::cout << "extern const std::string " << upper_name << "_MIN;" << std::endl; //<< " = \"" << name << ".min" << "\";" << std::endl;
			//std::cout << "extern const std::string " << upper_name << "_MAX;" << std::endl; //<< " = \"" << name << ".max" << "\";" << std::endl;
		}
	}
}

}

