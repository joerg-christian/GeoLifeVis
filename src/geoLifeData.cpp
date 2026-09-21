#include "geoLifeData.h"
namespace GeoLife
{
	MeansOfTransport convertLabel(std::string input)
	{

		if (input == "walk") return GeoLife::WALK;
		else if (input == "bike") return BIKE;
		else if (input == "bus") return BUS;
		else if (input == "car") return CAR;
		else if (input == "taxi") return CAR;
		else if (input == "subway") return SUBWAY;
		else if (input == "train") return TRAIN;
		else if (input == "airplane") return AIRPLANE;
		else if (input == "boat") return BOAT;
		else if (input == "run") return WALK;
		else if (input == "motorcycle") return MOTORCYCLE;
		else return NA;
	};

	int getLastDay(const int& month, const int& year)
	{
		// 2008 and 2012 are leap years
		int daysInFebruary = (year == 2008 || year == 2012) ? 29 : 28;
		if (month % 2 != 0) return 31;
		if (month == 2) return daysInFebruary;;
		return 30;
	}

	std::vector<std::string> getColumns(std::string str, const std::vector<std::string>& separators)
	{
		std::vector<std::string> columnEntries;
		unsigned int endOf1stCol = 0;
		for (int i = 0; i < separators.size(); i++)
		{
			if (separators[i] == "\n") columnEntries.push_back(str);
			else
			{
				endOf1stCol = str.find(separators[i]);
				if (endOf1stCol == std::string::npos) return columnEntries;	// cannot find separator >> return results so far
				columnEntries.push_back(str.substr(0, endOf1stCol));			// get entry in 1st column
				str.erase(0, endOf1stCol + 1);								// remove 1st column from line
			}
		}
		return columnEntries;
	}

	unsigned int GeoLifeDataSet::getNumberOfDataPoints()
	{
		return m_dataset.size();
	}

	std::size_t GeoLifeDataSet::getDataSetSize()
	{
		return m_dataset.size() * sizeof(GeoLifeData);
	}

	std::vector<GeoLifeData>* GeoLifeDataSet::getDataSet()
	{
		return &m_dataset;
	}

	unsigned int GeoLifeDataSet::loadDataSetFromFile(const std::string path, float fractionOfDataToBeLoaded)
	{
		std::ifstream file;
		file.open(path);
		if (!file.is_open()) std::cout << "\nERROR COULD NOT OPEN FILE FOR LOADING: " << path;
		std::string line;
		float boundaries[4];

		// First 4 lines are long lat boundaries: minLat MaxLat minLong max Long
		for (int i = 0; i < 4; i++)
		{
			std::getline(file, line);
			if (line.empty()) break;
			boundaries[i] = std::stof(line);
		}

		if (line.empty()) return false;
		m_minLat = boundaries[0];
		m_maxLat = boundaries[1];
		m_minLong = boundaries[2];
		m_maxLong = boundaries[3];

		// read lines with data entries
		unsigned int lineCount = 0;
		const unsigned int totalNumberOfLines = 18238686; // this is hardcoded after outputing number of data points manually
		unsigned int numberOfLinesToBeLoaded = fractionOfDataToBeLoaded <= 0.999999? static_cast<unsigned int>(fractionOfDataToBeLoaded * (float) totalNumberOfLines): 18238686;
		std::string loadingProgress = "0%";
		std::cout << "\nLoading GeoLife Dataset from txt.file. Progress: " << loadingProgress;
		while (file)
		{
			// Read data tokesn from file : x,y,year,month,day,hour,minute,second, label
			std::getline(file, line);
			if (line.empty()) break; // end of file reached
			std::vector<std::string> columns = getColumns(line, std::vector<std::string>({ "\t","\t", "\t", "\t", "\t", "\t", "\t","\t", "\n" }));
			if (columns.size() != 9) break; // something went wrong reading the line
			GeoLifeData dataPoint;
			dataPoint.x = std::stof(columns[0]);
			dataPoint.y = stof(columns[1]);
			dataPoint.year = stoi(columns[2]);
			dataPoint.month = stoi(columns[3]);
			dataPoint.day = stoi(columns[4]);
			dataPoint.hour = stoi(columns[5]);
			dataPoint.minute = stoi(columns[6]);
			dataPoint.second = stoi(columns[7]);
			dataPoint.label = static_cast<MeansOfTransport>(stoi(columns[8]));
			dataPoint.weekday = getWeekday(dataPoint.day, dataPoint.month, dataPoint.year);
			m_dataset.push_back(dataPoint);
			lineCount++;

			// Update Loading Process Output
			if (lineCount % 99999 == 0) // Only update after 99999 steps to make it smoother
			{
				const int decimals = 0;
				for (int i = 0; i < loadingProgress.size(); i++) std::cout << "\b";
				float percentage = 100 * ((float)lineCount / (float)numberOfLinesToBeLoaded);
				if (percentage >= 99.5) percentage = 100;
				std::stringstream ssPercentage;
				ssPercentage << std::fixed << std::setprecision(decimals) << percentage;
				loadingProgress = ssPercentage.str() + "%";
				std::cout << loadingProgress;
			}
			if (lineCount >= numberOfLinesToBeLoaded) break;
		}
		file.close();

		// Update Loading Process Output to 100%
		for (int i = 0; i < loadingProgress.size(); i++) std::cout << "\b";
		std::cout << "100%";
		std::cout << "\n# Datapoints = " << m_dataset.size();
		std::cout << "\n----------\n";
		return m_dataset.size();
	}

	void GeoLifeDataSet::readFromFolder(const std::string& path)
	{
		std::vector<LabelLogEntry> labelLogs;

		// find labels.txt file in the current folder
		for (const auto& folderEntry : std::filesystem::directory_iterator(path))
			if (folderEntry.path().filename() == "labels.txt") labelLogs = getLabelLogs(folderEntry.path().string());
		
		// iterate trough folders entries again to find folder with plt files
		for (const auto& folderEntry : std::filesystem::directory_iterator(path))
		{
			// current entry refers to a directory
			if (folderEntry.is_directory())
				readFromFolder(folderEntry.path().string());

			// Current Entry is not plt file: Skip entry
			if (folderEntry.path().extension() != ".plt") continue;

			// Current entry is plt file: Read content
			std::vector<GeoLifeData> dataPoints = readDataPointsFromPLT(folderEntry.path(), labelLogs);
			if (dataPoints.size() > 0) m_dataset.insert(m_dataset.end(), dataPoints.begin(), dataPoints.end());
		}
		labelLogs.clear();
		return;
	}

	std::vector<LabelLogEntry> GeoLifeDataSet::getLabelLogs(std::string filePath)
	{
		std::ifstream file(filePath);
		std::string line;
		std::vector<LabelLogEntry> logsPerDay;

		// Skip header line
		std::getline(file, line);

		// Go through file line by line and extract data
		while (file)
		{
			// get new line and if end of file is reached break
			std::getline(file, line);
			if (line.empty()) break;

			// We extract each entry as a seperate string
			std::vector<std::string> columnEntries = getColumns(line, std::vector<std::string>({ "/","/"," ",":",":","\t","/","/"," ",":",":","\t","\n" }));
			LabelLogEntry log;
			log.start.year = std::stoi(columnEntries[0]);
			log.start.month = std::stoi(columnEntries[1]);
			log.start.day = std::stoi(columnEntries[2]);
			log.start.hour = std::stoi(columnEntries[3]);
			log.start.minute = std::stoi(columnEntries[4]);
			log.start.second = std::stoi(columnEntries[5]);
			log.end.year = std::stoi(columnEntries[6]);
			log.end.month = std::stoi(columnEntries[7]);
			log.end.day = std::stoi(columnEntries[8]);
			log.end.hour = std::stoi(columnEntries[9]);
			log.end.minute = std::stoi(columnEntries[10]);
			log.end.second = std::stoi(columnEntries[11]);
			log.label = convertLabel(columnEntries[12]);

			// Split activities over many years into one per year
			std::vector<LabelLogEntry> logsPerYear({ log });
			bool splitRequired = true;
			while (splitRequired)
			{
				if (logsPerYear.back().start.year == logsPerYear.back().end.year) splitRequired = false;
				else
				{
					LabelLogEntry oldLog = logsPerYear.back();
					logsPerYear.pop_back();
					LabelLogEntry newLog = oldLog;
					oldLog.end.year = oldLog.start.year;
					oldLog.end.month = 12;
					oldLog.end.day = 31;
					oldLog.end.hour = 23;
					oldLog.end.minute = 59;
					oldLog.end.second = 59;
					newLog.start.year = oldLog.start.year + 1;
					newLog.start.month = 1;
					newLog.start.day = 1;
					newLog.start.hour = 0;
					newLog.start.minute = 0;
					newLog.start.second = 0;
					logsPerYear.push_back(oldLog);
					logsPerYear.push_back(newLog);
				}
			}

			// Split activities over many months into one per month
			std::vector<LabelLogEntry> logsPerMonth;
			for (auto logOverMonths : logsPerYear)
			{
				// If log is within a month write it to new vector and continue with next loop entry
				if (logOverMonths.start.month == logOverMonths.end.month)
				{
					logsPerMonth.push_back(logOverMonths);
					continue;
				}

				//If log over more than one months: chop it up into logs all within a single months, then write chopped logs to new vector
				std::vector<LabelLogEntry> choppedUpLog({ logOverMonths });
				splitRequired = true;
				while (splitRequired)
				{
					// take last log in vector, replace it by log within its first months followed by log over remianing months
					// THis loop runs until last log in vector is within a month
					LabelLogEntry oldLog = choppedUpLog.back();
					choppedUpLog.pop_back();
					LabelLogEntry newLog = oldLog;
					oldLog.end.month = oldLog.start.month;
					oldLog.end.day = getLastDay(oldLog.end.month, oldLog.end.year);
					oldLog.end.hour = 23;
					oldLog.end.minute = 59;
					oldLog.end.second = 59;
					newLog.start.month = oldLog.start.month + 1; // we already removes logs that go over new years eve, all logs end before that
					newLog.start.day = 1;
					newLog.start.hour = 0;
					newLog.start.minute = 0;
					newLog.start.second = 0;
					choppedUpLog.push_back(oldLog);
					choppedUpLog.push_back(newLog);
					if (newLog.start.month == newLog.end.month) splitRequired = false; // if new log ist intra month we are done splitting
				}
				logsPerMonth.insert(logsPerMonth.end(), choppedUpLog.begin(), choppedUpLog.end());
			}

			// Split activities over many days into one per day
			for (auto logOverDays : logsPerMonth)
			{
				// If log is within a day write it to new vector and continue with next loop entry
				if (logOverDays.start.month == logOverDays.end.month)
				{
					logsPerDay.push_back(logOverDays);
					continue;
				}

				//If log over more than one days: chop it up into logs all within a single day, then write chopped logs to new vector
				std::vector<LabelLogEntry> choppedUpLog({ logOverDays });
				splitRequired = true;
				while (splitRequired)
				{
					// take last log in vector, replace it by log within its first months followed by log over remianing months
					// This loop runs until last log in vector is within a month
					LabelLogEntry oldLog = choppedUpLog.back();
					choppedUpLog.pop_back();
					LabelLogEntry newLog = oldLog;
					oldLog.end.day = oldLog.start.day;
					oldLog.end.hour = 23;
					oldLog.end.minute = 59;
					oldLog.end.second = 59;
					newLog.start.day = oldLog.end.day + 1; // all logs end befeore the end of the month already!
					newLog.start.hour = 0;
					newLog.start.minute = 0;
					newLog.start.second = 0;
					choppedUpLog.push_back(oldLog);
					choppedUpLog.push_back(newLog);
					if (newLog.start.day == newLog.end.day) splitRequired = false; // if new log ist intra month we are done splitting
				}
				logsPerDay.insert(logsPerDay.end(), choppedUpLog.begin(), choppedUpLog.end());
			}
		}
		file.close();
		return logsPerDay;
	}

	std::vector<GeoLifeData> GeoLifeDataSet::readDataPointsFromPLT(std::filesystem::path path, std::vector<LabelLogEntry> labelLogs)
	{
		std::ifstream file;
		file.open(path);
		if (!file.is_open()) std::cout << "\nERROR COULD NOT OPEN FILE " << path;
		std::vector<GeoLifeData> dataPoints;
		std::string line;

		// skip first 6 lines according to documentation pdf
		for (int i = 0; i < 6; i++)
			std::getline(file, line);

		// read lines with data entries
		while (file)
		{
			// Create DataPoint for new line
			GeoLifeData dataPoint;

			// get new line and break if line is end of file
			std::getline(file, line);
			if (line.empty()) break; // line is End of file

			// Get data from each column in the line (,komma-separated columns)
			size_t pos = 0;
			std::string columns[7];
			for (int i = 0; i < 7; i++)
			{
				pos = line.find(",");
				columns[i] = line.substr(0, pos);
				line.erase(0, pos + 1);
			}

			// get longitude & latitude: if outside map, skip dataPoint, else normalize and save it
			float latitude = std::stof(columns[0]);
			float longitude = std::stof(columns[1]);
			if (latitude < m_minLat || longitude < m_minLong || latitude > m_maxLat || longitude > m_maxLong) continue;
			dataPoint.y = (latitude - m_minLat) / (m_maxLat - m_minLat);		// normalized
			dataPoint.x = (longitude - m_minLong) / (m_maxLong - m_minLong);	// normalized


			// get date
			std::vector<std::string> yearMonthDay = getColumns(columns[5], std::vector<std::string>({ "-","-","\n" }));
			dataPoint.year = std::stoi(yearMonthDay[0]);
			dataPoint.month = std::stoi(yearMonthDay[1]);
			dataPoint.day = std::stoi(yearMonthDay[2]);

			// get time
			std::vector<std::string> hourMinSec = getColumns(columns[6], std::vector<std::string>({ ":",":","\n" }));

			dataPoint.hour = std::stoi(hourMinSec[0]);
			dataPoint.minute = std::stoi(hourMinSec[1]);
			dataPoint.second = std::stoi(hourMinSec[2]);

			// Find Label for this data point, use NA if no labels available
			dataPoint.label = (int)NA;
			if (labelLogs.empty())
			{
				dataPoints.push_back(dataPoint);
				continue;
			}
			for (LabelLogEntry log : labelLogs)
			{
				if (log.start.year != dataPoint.year) continue;
				if (log.start.month != dataPoint.month) continue;
				if (log.start.day != dataPoint.day) continue;
				int logStartInSeconds = (log.start.hour * 3600) + (log.start.minute * 60) + (log.start.second);
				int logEndInSeconds = (log.end.hour * 3600) + (log.end.minute * 60) + (log.end.second);
				int dataPointTimeInSeconds = (dataPoint.hour * 3600) + (dataPoint.minute * 60) + (dataPoint.second);
				if (logStartInSeconds > dataPointTimeInSeconds) continue;
				if (dataPointTimeInSeconds < logEndInSeconds) dataPoint.label = (int)log.label;
			}
			dataPoints.push_back(dataPoint);
		}

		// Close File and return data Points
		file.close();
		return dataPoints;
	}

	bool GeoLifeDataSet::saveDataSetToFile()
	{
		// Check if evrything is ready for writing data
		if (m_dataset.empty())
		{
			std::cout << "\nERROR: Trying to write empty dataset to file.";
			return false;
		}
		std::ofstream outputFile("geoLifeDataSet.txt");
		if (!outputFile.is_open())
		{
			std::cout << "\nERROR: Cannot open file geoLifeDataSet.txt ";
			return false;
		}

		// Write min/max lat, long to first 4 lines of file
		outputFile << m_minLat << "\n" << m_maxLat << "\n" << m_minLong << "\n" << m_maxLong << "\n";

		// Loop through data Points and write them to file line by line
		for (GeoLifeData dataPoint : m_dataset)
		{
			outputFile << dataPoint.x << "\t" << dataPoint.y << "\t";
			outputFile << dataPoint.year << "\t" << dataPoint.month << "\t" << dataPoint.day << "\t";
			outputFile << dataPoint.hour << "\t" << dataPoint.minute << "\t" << dataPoint.second << "\t" << dataPoint.label << "\n";
		}

		// Close file
		outputFile.close();
		return true;
	}

	bool parametersChanged(const UserParameters& oldParameters, UserParameters& newParameters)
	{
		//unsigned int maxIndexDataSet = 18238685; // this is the maximum index of the geoleife datapoints whihc depends on how many points we load from the file
		//unsigned int resX = 1800;
		//unsigned int resY = 1800;
		if (oldParameters.kernelSize != newParameters.kernelSize || oldParameters.sigma != newParameters.sigma)
		{
			newParameters.kernelChanged = true;
			return true;
		}
		else newParameters.kernelChanged = true;
		if (oldParameters.yearMin != newParameters.yearMin) return true;
		if (oldParameters.yearMax != newParameters.yearMax) return true;
		if (oldParameters.monthMin != newParameters.monthMin) return true;
		if (oldParameters.monthMax != newParameters.monthMax) return true;
		if (oldParameters.dayMin != newParameters.dayMin) return true;
		if (oldParameters.dayMax != newParameters.dayMax) return true;
		if (oldParameters.hourMin != newParameters.hourMin) return true;
		if (oldParameters.hourMax != newParameters.hourMax) return true;
		if (oldParameters.kernelChanged != newParameters.kernelChanged) return true;
		if (oldParameters.labelNA != newParameters.labelNA) return true;
		if (oldParameters.labelWALK != newParameters.labelWALK) return true;
		if (oldParameters.labelBIKE != newParameters.labelBIKE) return true;
		if (oldParameters.labelBUS != newParameters.labelBUS) return true;
		if (oldParameters.labelCAR != newParameters.labelCAR) return true;
		if (oldParameters.labelSUBWAY != newParameters.labelSUBWAY) return true;
		if (oldParameters.labelTRAIN != newParameters.labelTRAIN) return true;
		if (oldParameters.labelAIRPLANE != newParameters.labelAIRPLANE) return true;
		if (oldParameters.labelBOAT != newParameters.labelBOAT) return true;
		if (oldParameters.labelMOTORCYCLE != newParameters.labelMOTORCYCLE) return true;
		if (oldParameters.labelMonday != newParameters.labelMonday) return true;
		if (oldParameters.labelTuesday != newParameters.labelTuesday) return true;
		if (oldParameters.labelWednesday != newParameters.labelWednesday) return true;
		if (oldParameters.labelThursday != newParameters.labelThursday) return true;
		if (oldParameters.labelFriday != newParameters.labelFriday) return true;
		if (oldParameters.labelSaturday != newParameters.labelSaturday) return true;
		if (oldParameters.labelSunday != newParameters.labelSunday) return true;
		if (oldParameters.zoom != newParameters.zoom) return true;
		return false;
	}

	// Shift by +8 hours
	void convertToBejingTime(std::vector<GeoLifeData>& inputData)
	{
		std::vector<GeoLifeData> shiftedData;
		for (GeoLifeData& dataPoint : inputData)
		{
			// if time shift stays on same day just increase hours
			if (dataPoint.hour + 8 % 24 > dataPoint.hour) 
				dataPoint.hour += 8;
			else // increase day
			{
				dataPoint.hour = (dataPoint.hour + 8 ) % 24;
				
				// if time shift stays on same month just increase day
				if(dataPoint.day != getLastDay(dataPoint.month, dataPoint.year))
					dataPoint.day +=1;
				else // last day of month: increase month
				{
					dataPoint.day = 1;
					// if time shift stays on same year just increase month
					if (12 != dataPoint.month)
						dataPoint.month += 1;
					else //increase year
					{
						dataPoint.month = 1;
						dataPoint.year += 1;
					}
				}

			}
		}
	}

	// Helper: convert date to weekday 1 = monday 2 = tuesday ... 7 = sunday
	unsigned int getWeekday(int day, int month, int year) {
		// Adjust month and year for Zeller's algorithm
		if (month < 3) {
			month += 12;
			year -= 1;
		}

		int K = year % 100;   // Year of the century
		int J = year / 100;   // Zero-based century

		// Zeller's Congruence formula to calculate the day of the week
		int f = day + (13 * (month + 1)) / 5 + K + (K / 4) + (J / 4) - (2 * J);
		int weekday = f % 7;

		// Correcting if the weekday is negative
		if (weekday < 0) {
			weekday += 7;
		}

		// Array of weekday names
		int days[] = { 6, 7, 1, 2, 3, 4, 5 };
		return days[weekday];
	}

}