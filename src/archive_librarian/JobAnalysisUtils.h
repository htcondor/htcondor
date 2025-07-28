#pragma once

#include "JobQueryStructures.h"
#include <string>
#include <vector>
#include <optional>

namespace JobAnalysisUtils {

    struct AnalysisFlags {
        bool usage = false;
        bool batch = false;
        bool dag = false;
        bool files = false;
        bool when = false;
        bool where = false;
        
        bool hasAnyFlag() const {
            return usage || batch || dag || files || when || where;
        }
    };

    struct CommandOptions {
        std::string username;
        int clusterId = -1;
        AnalysisFlags flags;
        bool showCacheStats = false;
        bool clearCache = false;
    };

    // Parsing Class Ads
    std::optional<ParsedJobRecord> parseClassAd(const std::string& classAdText);
    std::vector<ParsedJobRecord> parseClassAds(const std::vector<std::string>& rawClassAds);


    // Command line parsing utility
    CommandOptions parseArguments(int argc, char* argv[]);

    // Output formatting utilities - called by Librarian::query()
    void printBaseOutput(const std::vector<ParsedJobRecord>& jobs, 
                        const std::string& username, int clusterId);

    void printAnalysisOutput(const std::vector<ParsedJobRecord>& jobs, 
                        const AnalysisFlags& flags,
                        const std::string& username, int clusterId);

    void printManual();

    void printError(const std::string& errorMessage);

    void printNoJobsFound(const std::string& username, int clusterId);

} // namespace JobAnalysisUtils