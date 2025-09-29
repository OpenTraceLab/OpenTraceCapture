#include <config.h>
#include <string>
#include <glibmm.h>
#include "libopentracecapturecxx/libopentracecapturecxx.hpp"


const DataType *ConfigKey::data_type() const
{
	const struct otc_key_info *info = otc_key_info_get(OTC_KEY_CONFIG, id());
	if (!info)
		throw Error(OTC_ERR_NA);
	return DataType::get(info->datatype);
}

std::string ConfigKey::identifier() const
{
	const struct otc_key_info *info = otc_key_info_get(OTC_KEY_CONFIG, id());
	if (!info)
		throw Error(OTC_ERR_NA);
	return valid_string(info->id);
}

std::string ConfigKey::description() const
{
	const struct otc_key_info *info = otc_key_info_get(OTC_KEY_CONFIG, id());
	if (!info)
		throw Error(OTC_ERR_NA);
	return valid_string(info->name);
}

const ConfigKey *ConfigKey::get_by_identifier(std::string identifier)
{
	const struct otc_key_info *info = otc_key_info_name_get(OTC_KEY_CONFIG, identifier.c_str());
	if (!info)
		throw Error(OTC_ERR_ARG);
	return get(info->key);
}

#ifndef HAVE_STOI_STOD

/* Fallback implementation of stoi and stod */

#include <cstdlib>
#include <cerrno>
#include <stdexcept>
#include <limits>

static inline int stoi( const std::string& str )
{
	char *endptr;
	errno = 0;
	const long ret = std::strtol(str.c_str(), &endptr, 10);
	if (endptr == str.c_str())
		throw std::invalid_argument("stoi");
	else if (errno == ERANGE ||
		 ret < std::numeric_limits<int>::min() ||
		 ret > std::numeric_limits<int>::max())
		throw std::out_of_range("stoi");
	else
		return ret;
}

static inline double stod( const std::string& str )
{
	char *endptr;
	errno = 0;
	const double ret = std::strtod(str.c_str(), &endptr);
	if (endptr == str.c_str())
		throw std::invalid_argument("stod");
	else if (errno == ERANGE)
		throw std::out_of_range("stod");
	else
		return ret;
}
#endif

#ifndef HAVE_STOUL

/* Fallback implementation of stoul. */

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static inline unsigned long stoul(const std::string &str)
{
	char *endptr;
	unsigned long ret;
	errno = 0;
	ret = std::strtoul(str.c_str(), &endptr, 10);
	if (endptr == str.c_str())
		throw std::invalid_argument("stoul");
	/*
	 * TODO Convert to a larger/wider intermediate data type?
	 * Because after conversion into the very target type, the
	 * range check is assumed to be ineffective.
	 */
	if (errno == ERANGE ||
		ret < std::numeric_limits<unsigned long>::min() ||
		ret > std::numeric_limits<unsigned long>::max())
		throw std::out_of_range("stoul");
	return ret;
}
#endif

// Conversion from text to uint32_t, including a range check.
// This is sigrok specific, _not_ part of any C++ standard library.
static uint32_t stou32(const std::string &str)
{
	unsigned long ret;
	errno = 0;
	ret = stoul(str);
	if (errno == ERANGE)
		throw std::out_of_range("stou32");
	if (ret > std::numeric_limits<uint32_t>::max())
		throw std::out_of_range("stou32");
	return ret;
}

Glib::VariantBase ConfigKey::parse_string(std::string value, enum otc_datatype dt)
{
	GVariant *variant;
	uint64_t p, q;

	switch (dt)
	{
		case OTC_T_UINT64:
			check(otc_parse_sizestring(value.c_str(), &p));
			variant = g_variant_new_uint64(p);
			break;
		case OTC_T_STRING:
			variant = g_variant_new_string(value.c_str());
			break;
		case OTC_T_BOOL:
			variant = g_variant_new_boolean(otc_parse_boolstring(value.c_str()));
			break;
		case OTC_T_FLOAT:
			try {
				variant = g_variant_new_double(stod(value));
			} catch (invalid_argument&) {
				throw Error(OTC_ERR_ARG);
			}
			break;
		case OTC_T_RATIONAL_PERIOD:
			check(otc_parse_period(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case OTC_T_RATIONAL_VOLT:
			check(otc_parse_voltage(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case OTC_T_INT32:
			try {
				variant = g_variant_new_int32(stoi(value));
			} catch (invalid_argument&) {
				throw Error(OTC_ERR_ARG);
			}
			break;
		case OTC_T_UINT32:
			try {
				variant = g_variant_new_uint32(stou32(value));
			} catch (invalid_argument&) {
				throw Error(OTC_ERR_ARG);
			}
			break;
		default:
			throw Error(OTC_ERR_BUG);
	}

	return Glib::VariantBase(variant, false);
}

Glib::VariantBase ConfigKey::parse_string(std::string value) const
{
	enum otc_datatype dt = (enum otc_datatype)(data_type()->id());
	return parse_string(value, dt);
}
