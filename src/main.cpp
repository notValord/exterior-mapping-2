#include <application.hpp>

const std::string TEST_DIR = "../tests/";

bool parseArgs(int argc, char** argv, std::string& testFile, std::string& outputFile) {
     for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--runtest") {
            if (i + 2 >= argc) {
                std::cerr << "Usage: " << argv[0] << " --runtest <testFile> <outputFile>\n";
                return false;
            }

            testFile = argv[i + 1];
            outputFile = argv[i + 2];

            std::cout << "Running test:\n";
            std::cout << "Input: " << testFile << "\n";
            std::cout << "Output: " << outputFile << "\n";
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: " << argv[0] << " --runtest <testFile> <outputFile>\n";
            return false;
        }

        i += 2; // Skip the next two arguments since we've already processed them
    }
    return true;
}

void runTest(const std::string& testFile, const std::string& outputFile, App& app) {
    std::ifstream inputFile(TEST_DIR + testFile);
    if (!inputFile.is_open()) {
        std::cerr << "Could not open input file: " << testFile << std::endl;
        return; 
    }

    std::ofstream outFile(TEST_DIR + outputFile);
    if(!outFile.is_open()) {
        std::cerr << "Could not open output file: " << outputFile << std::endl;
        return; 
    }
    
    json testJ;
    inputFile >> testJ;
    
    std::string setup = testJ["setup"];
    bool precision = testJ["precision"];

    std::cout << "Setup: " << setup << "\n";
    std::cout << "Precision: " << precision << "\n";

    app.setupTest(outFile, setup, precision);

    // iterate tests
    for (const auto& test : testJ["tests"]) {
        TestInfo testInfo;
        testInfo.algorithm = test["alg"];

        // nullable fields
        testInfo.fov = test["fov"].is_null() ? -1 : test["fov"].get<float>();
        testInfo.sampleCount = test["sampleCount"].is_null() ? -1 : test["sampleCount"].get<int>();
        testInfo.width = test["width"].is_null() ? 0 : test["width"].get<int>();
        testInfo.height = test["height"].is_null() ? 0 : test["height"].get<int>();

        std::cout << "\nTest:\n";
        // std::cout << "  alg: " << testInfo.algorithm << "\n";
        // std::cout << "  fov: " << testInfo.fov << "\n";
        // std::cout << "  sampleCount: " << testInfo.sampleCount << "\n";
        // std::cout << "  width: " << testInfo.width << "\n";
        // std::cout << "  height: " << testInfo.height << "\n";

        app.runTest(outFile, testInfo);
    }

    inputFile.close();
    outFile.close();
}

int main(int argc, char** argv) {
    if (argc == 1) {
        // No arguments provided, run the application normally
        App app;

        try {
            app.run();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        std::string testFile, outputFile;
        if (!parseArgs(argc, argv, testFile, outputFile)) {
            return EXIT_FAILURE;
        }

        App app;

        runTest(testFile, outputFile, app);        
    }

    return EXIT_SUCCESS;
}