/*Copyright (c) 2016 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/



#include "HoI4State.h"
#include <fstream>
#include <random>
#include "../Configuration.h"
#include "../Mappers/CoastalHoI4Provinces.h"
#include "../Mappers/ProvinceMapper.h"
#include "../Mappers/V2Localisations.h"
#include "../V2World/V2Province.h"
#include "../V2World/V2World.h"
#include "Log.h"
#include "OSCompatibilityLayer.h"



HoI4State::HoI4State(const Vic2State* _sourceState, int _ID, string _ownerTag)
{
	sourceState = _sourceState;

	ID = _ID;
	provinces.clear();
	ownerTag = _ownerTag;
	capitalState = false;

	manpower = 0;

	civFactories = 0;
	milFactories = 0;
	dockyards = 0;
	category = "pastoral";
	infrastructure = 0;

	navalLevel = 0;
	navalLocation = 0;

	airbaseLevel = 0;

	resources.clear();

	victoryPointPosition = 0;
	victoryPointValue = 0;
}


void HoI4State::output(string _filename)
{
	// create the file
	string filename("Output/" + Configuration::getOutputName() + "/history/states/" + _filename);
	ofstream out(filename);
	if (!out.is_open())
	{
		LOG(LogLevel::Error) << "Could not open \"output/" + Configuration::getOutputName() + "/history/states/" + _filename;
		exit(-1);
	}

	// output the data
	out << "state={" << endl;
	out << "\tid=" << ID << endl;
	out << "\tname= \"STATE_" << ID << "\"" << endl;
	out << "\tmanpower = " << manpower << endl;
	out << endl;
	if (resources.size() > 0)
	{
		out << "\tresources={" << endl;
		for (auto resource: resources)
		{
			out << "\t\t" << resource.first << " = " << resource.second << endl;
		}
		out << "\t}" << endl;
	}
	out << "\tstate_category = "<< category << endl;
	out << "" << endl;
	out << "\thistory={" << endl;
	out << "\t\towner = " << ownerTag << endl;
	if ((victoryPointValue > 0) && (victoryPointPosition != 0))
	{
		out << "\t\tvictory_points = {" << endl;
		out << "\t\t\t" << victoryPointPosition << " " << victoryPointValue << endl;
		out << "\t\t}" << endl;
	}
	out << "\t\tbuildings = {" << endl;
	out << "\t\t\tinfrastructure = "<< infrastructure << endl;
	out << "\t\t\tindustrial_complex = " << civFactories << endl;
	out << "\t\t\tarms_factory = " << milFactories << endl;
	if (dockyards > 0)
	{
		out << "\t\t\tdockyard = " << dockyards << endl;
	}
		
	if ((navalLevel > 0) && (navalLocation > 0))
	{
		out << "\t\t\t" << navalLocation << " = {" << endl;
		out << "\t\t\t\tnaval_base = " << navalLevel << endl;
		out << "\t\t\t}" << endl;
	}
	out << "\t\t\tair_base = "<< airbaseLevel << endl;
	out << "\t\t}" << endl;
	//out << "\t}" << endl;
	for (auto core: cores)
	{
		out << "\t\tadd_core_of = " << core << endl;
	}
	out << "\t}" << endl;
	out << endl;
	out << "\tprovinces={" << endl;
	out << "\t\t";
	for (auto provnum : provinces)
	{
		out << provnum << " ";
	}
	out << endl;
	out << "\t}" << endl;
	out << "}" << endl;

	out.close();
}


void HoI4State::setNavalBase(int level, int location)
{
	if (provinces.find(location) != provinces.end())
	{
		navalLevel		= level;
		navalLocation	= location;
	}
}


void HoI4State::addCores(const vector<string>& newCores)
{
	for (auto newCore: newCores)
	{
		cores.insert(newCore);
	}
}


void HoI4State::assignVP(int location)
{
	victoryPointPosition = location;

	victoryPointValue = 1;
	if (cores.count(ownerTag) != 0)
	{
		victoryPointValue += 2;
	}
}


bool HoI4State::tryToCreateVP()
{
	for (auto vic2Province: sourceState->getProvinceNums())
	{
		auto provMapping = provinceMapper::getVic2ToHoI4ProvinceMapping().find(vic2Province);
		if (
			 (provMapping != provinceMapper::getVic2ToHoI4ProvinceMapping().end()) &&
			 (isProvinceInState(provMapping->second[0]))
			)
		{
			assignVP(provMapping->second[0]);
			return true;
		}
	}

	return false;
}


void HoI4State::convertIndustry(double workerFactoryRatio)
{
	int factories = determineFactoryNumbers(workerFactoryRatio);

	determineCategory(factories);
	setInfrastructure(factories);
	setIndustry(factories);
	addVictoryPointValue(factories / 2);
}


int HoI4State::determineFactoryNumbers(double workerFactoryRatio)
{
	double rawFactories = sourceState->getEmployedWorkers() * workerFactoryRatio;
	rawFactories = round(rawFactories);
	return constrainFactoryNumbers(rawFactories);
}


int HoI4State::constrainFactoryNumbers(double rawFactories)
{
	int factories = static_cast<int>(rawFactories);

	int upperLimit = 12;
	if (capitalState)
	{
		upperLimit = 11;
	}

	if (factories < 0)
	{
		factories = 0;
	}
	else if (factories > upperLimit)
	{
		factories = upperLimit;
	}

	return factories;
}


void HoI4State::determineCategory(int factories)
{
	if (capitalState)
	{
		factories++;
	}

	int population = sourceState->getPopulation();

	int stateSlots = population / 120000; // one slot is given per 120,000 people (need to change)
	if (factories >= stateSlots)
	{
		stateSlots = factories + 2;
	}

	for (auto possibleCategory: getStateCategories())
	{
		if (stateSlots >= possibleCategory.first)
		{
			category = possibleCategory.second;
		}
	}
}


map<int, string> HoI4State::getStateCategories()
{
	map<int, string> stateCategories;

	stateCategories.insert(make_pair(12, "megalopolis"));
	stateCategories.insert(make_pair(10, "metropolis"));
	stateCategories.insert(make_pair(8, "large_city"));
	stateCategories.insert(make_pair(6, "city"));
	stateCategories.insert(make_pair(5, "large_town"));
	stateCategories.insert(make_pair(4, "town"));
	stateCategories.insert(make_pair(2, "rural"));
	stateCategories.insert(make_pair(1, "pastoral"));
	stateCategories.insert(make_pair(0, "enclave"));

	return stateCategories;
}


void HoI4State::setInfrastructure(int factories)
{
	infrastructure = sourceState->getAverageRailLevel();

	if (factories > 4)
	{
		infrastructure++;
	}
	if (factories > 6)
	{
		infrastructure++;
	}
	if (factories > 10)
	{
		infrastructure++;
	}
}


static mt19937 randomnessEngine;
static uniform_int_distribution<> numberDistributor(0, 99);
void HoI4State::setIndustry(int factories)
{
	if (amICoastal())
	{
		// distribute military factories, civilian factories, and dockyards using unseeded random
		//		20% chance of dockyard
		//		57% chance of civilian factory
		//		23% chance of military factory
		for (int i = 0; i < factories; i++)
		{
			double randomNum = numberDistributor(randomnessEngine);
			if (randomNum > 76)
			{
				milFactories++;
			}
			else if (randomNum > 19)
			{
				civFactories++;
			}
			else
			{
				dockyards++;
			}
		}
	}
	else
	{
		// distribute military factories, civilian factories, and dockyards using unseeded random
		//		 0% chance of dockyard
		//		71% chance of civilian factory
		//		29% chance of military factory
		for (int i = 0; i < factories; i++)
		{
			double randomNum = numberDistributor(randomnessEngine);
			if (randomNum > 70)
			{
				milFactories++;
			}
			else
			{
				civFactories++;
			}
		}
	}
}


bool HoI4State::amICoastal()
{
	map<int, int> coastalProvinces = coastalProvincesMapper::getCoastalProvinces();
	for (auto province: provinces)
	{
		auto itr = coastalProvinces.find(province);
		if (itr != coastalProvinces.end())
		{
			return true;
		}
	}

	return false;
}


pair<string, string> HoI4State::makeLocalisation(const pair<const string, string>& Vic2NameInLanguage) const
{
	return make_pair(
		makeLocalisationKey(),
		makeLocalisationValue(Vic2NameInLanguage)
	);
}


string HoI4State::makeLocalisationKey() const
{
	return string("STATE_") + to_string(ID);
}


string HoI4State::makeLocalisationValue(const pair<const string, string>& Vic2NameInLanguage) const
{
	string localisedName = "";
	if (sourceState->isPartialState())
	{
		localisedName += V2Localisations::GetTextInLanguage(sourceState->getOwner() + "_ADJ", Vic2NameInLanguage.first) + " ";
	}
	localisedName += Vic2NameInLanguage.second;

	return localisedName;
}


pair<string, string> HoI4State::makeVPLocalisation(const pair<const string, string>& Vic2NameInLanguage) const
{
	return make_pair(
		"VICTORY_POINTS_" + to_string(victoryPointPosition),
		Vic2NameInLanguage.second
	);
}


bool HoI4State::isProvinceInState(int provinceNum)
{
	return (provinces.find(provinceNum) != provinces.end());
}