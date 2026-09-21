#ifndef GEOLIFEDATA_H
#define GEOLIFEDATA_H

#include <string>
#include <vector>
#include <iomanip>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <time.h>
#include <iostream>
#include <sstream>

namespace GeoLife
{
	enum MeansOfTransport
	{
		NA,
		WALK,
		BIKE,
		BUS,
		CAR,
		SUBWAY,
		TRAIN,
		AIRPLANE,
		BOAT,
		MOTORCYCLE
	};

	struct TimeStamp
	{
		unsigned int year;
		unsigned int month;
		unsigned int day;
		unsigned int hour;
		unsigned int minute;
		unsigned int second;
	};

	struct LabelLogEntry
	{
		MeansOfTransport label;
		TimeStamp start;
		TimeStamp end;
	};

	struct GeoLifeData
	{
		float x;
		float y;
		unsigned int year;
		unsigned int month;
		unsigned int day;
		unsigned int weekday;
		unsigned int hour;
		unsigned int minute;
		unsigned int second;
		int label;
	};

	struct UserParameters
	{
		unsigned int maxIndexDataSet = 18238685; // this is the maximum index of the geoleife datapoints whihc depends on how many points we load from the file
		unsigned int resX = 1280;
		unsigned int resY = 1280;
		unsigned int runCompute = 1;
		int kernelSize = 3;
		int yearMin = 2007;
		int yearMax = 2013;
		int monthMin= 0;
		int monthMax = 12;
		int dayMin = 1;
		int dayMax = 31;
		int hourMin = 0;
		int hourMax = 23;
		float sigma = 1;
		int kernelChanged = 0;
		int labelNA;
		int labelWALK;
		int labelBIKE;
		int labelBUS;
		int labelCAR;
		int labelSUBWAY;
		int labelTRAIN;
		int labelAIRPLANE;
		int labelBOAT;
		int labelMOTORCYCLE;
		int labelMonday;
		int labelTuesday;
		int labelWednesday;
		int labelThursday;
		int labelFriday;
		int labelSaturday;
		int labelSunday;
		int numberOfWorkGroups;
		float zoom = 1.f; // A value between 0 and 1. The whole map has sidelength 1. No zoom means m_zoom = 1. Maximum Zoom means m_zoom = 0.1
		float originX;
		float originY;
	};
	// Maps for each transportation mode its names as string to corresponding enum 
	MeansOfTransport convertLabel(std::string input);

	//returns last day of month
	int getLastDay(const int& month, const int& year);

	// splits string into vector of strings between provided separators
	std::vector<std::string> getColumns(std::string str, const std::vector<std::string>& separators);

	// shifts time by 8 hours (from GMT to Bejing)
	void convertToBejingTime(std::vector<GeoLifeData>& inputData);

	// get day of the week from date 1 = Monday ... 7 = Sunday, uses Zeller's Congruence
	unsigned int getWeekday(int day, int month, int year);

	class GeoLifeDataSet
	{
	public:

		GeoLifeDataSet(const std::string& path, const float& minLat, const float& maxLat, const float& minLong, const float& maxLong) :
			m_debugStartTime(time(0)), m_minLat(minLat), m_minLong(minLong), m_maxLong(maxLong), m_maxLat(maxLat)
		{
			readFromFolder(path);
			saveDataSetToFile();
		}

		GeoLifeDataSet(const std::string& path,unsigned int* numberOfDataPointsLoaded, float fractionOfDataTobeLoaded = 1.0f)
		{
			*numberOfDataPointsLoaded = loadDataSetFromFile(path, fractionOfDataTobeLoaded);
			convertToBejingTime(m_dataset);
		}

		unsigned int getNumberOfDataPoints();
		std::size_t getDataSetSize();
		std::vector<GeoLifeData>* getDataSet();

	private:
		std::vector<GeoLifeData> m_dataset;
		time_t m_debugStartTime;
		float m_minLat, m_minLong, m_maxLat, m_maxLong;

		// Reads Dataset from a folder with plt files and labels
		void readFromFolder(const std::string& path);

		// Returns label logs chopped up into intra-day logs
		std::vector<LabelLogEntry> getLabelLogs(std::string filePath);

		// loads datapoints from a plt-file
		std::vector<GeoLifeData> readDataPointsFromPLT(std::filesystem::path path, std::vector<LabelLogEntry> labelLogs);

		// Saves labeled dataSet to txt-file
		bool saveDataSetToFile();

		// Loads already labeled dataset from txt file, returns number of datapoints loaded
		unsigned int loadDataSetFromFile(const std::string path, float fractionOfDataToBeLoaded = 1.0f);
	};

	bool parametersChanged(const UserParameters& oldParameters, UserParameters& newParameters);
}
#endif // !GEOLIFEDATA_H
