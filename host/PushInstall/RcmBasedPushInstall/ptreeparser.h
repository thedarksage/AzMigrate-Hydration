/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	ptreeparser.h

Description	:   Ptree parsing functions.

+------------------------------------------------------------------------------------+
*/

#ifndef __PTREE_PARSER_H__
#define __PTREE_PARSER_H__

#include "RcmJobInputValidationFailedException.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace PI{

	template <typename T>
	T GetKeyValueFromPtree(boost::property_tree::ptree pt, std::string key)
	{
		if (!pt.get_child_optional(key))
		{
			throw RcmJobInputValidationFailedException(key);
		}
		else
		{
			return pt.get<T>(key);
		}
	}

	template <typename T>
	T GetKeyValueFromPtree(boost::property_tree::ptree pt, std::string key, T defaultValue)
	{
		if (!pt.get_child_optional(key))
		{
			return defaultValue;
		}
		else
		{
			return pt.get<T>(key);
		}
	}
}

#endif