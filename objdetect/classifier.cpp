#include "classifier.h"
#include "../utils/timer.h"
#include "../utils/visualizer.h"
#include "../core/classifier_criteria.h"
#include "../processing/processing.h"

namespace tless {
    void Classifier::train(std::string templatesListPath, std::string resultPath, std::vector<uint> indices) {
        std::ifstream ifs(templatesListPath);
        assert(ifs.is_open());

        // Init classifiers and parser
        Hasher hasher(criteria);
        Matcher matcher(criteria);
        Parser parser(criteria);

        // Init common
        std::ostringstream oss;
        std::vector<Template> templates, allTemplates;
        std::string path;

        Timer tTraining;
        std::cout << "Training... " << std::endl;

        while (ifs >> path) {
            std::cout << "  |_ " << path;

            // Parse each object by one and save it
            parser.parseObject(path, templates, indices);

            // Train features for loaded templates
            matcher.train(templates);

            // Save templates for later hash table generation
            allTemplates.insert(allTemplates.end(), templates.begin(), templates.end());

            // Persist trained data
            oss << resultPath << "trained_" << std::setw(2) << std::setfill('0') << templates[0].id / 2000 << ".yml.gz";
            std::string trainedPath = oss.str();
            cv::FileStorage fsw(trainedPath, cv::FileStorage::WRITE);

            // Save templates data
            fsw << "templates" << "[";
            for (auto &t : templates) {
                fsw << t;
            }
            fsw << "]";

            // Cleanup
            oss.str("");
            fsw.release();
            templates.clear();
            std::cout << " -> " << trainedPath << std::endl;
        }

        ifs.close();

        // Save classifier info
        cv::FileStorage fsw(resultPath + "classifier.yml.gz", cv::FileStorage::WRITE);
        fsw << "criteria" << *criteria;
        std::cout << "  |_ info -> " << resultPath + "classifier.yml.gz" << std::endl;

        // Train hash tables
        std::cout << "  |_ Training hash tables... " << std::endl;
        hasher.train(allTemplates, tables);
        assert(!tables.empty());
        std::cout << "    |_ " << tables.size() << " hash tables generated" << std::endl;

        // Persist hashTables
        fsw << "tables" << "[";
        for (auto &table : tables) {
            fsw << table;
        }
        fsw << "]";
        fsw.release();

        std::cout << "  |_ tables -> " << resultPath + "classifier.yml.gz" << std::endl;
        std::cout << "DONE!, took: " << tTraining.elapsed() << " s" << std::endl << std::endl;
    }

    void Classifier::load(const std::string &trainedTemplatesListPath, const std::string &trainedPath) {
        std::ifstream ifs(trainedTemplatesListPath);
        assert(ifs.is_open());

        Timer tLoading;
        std::string path;
        std::cout << "Loading trained templates... " << std::endl;

        while (ifs >> path) {
            std::cout << "  |_ " << path;

            // Load trained data
            cv::FileStorage fsr(path, cv::FileStorage::READ);
            cv::FileNode tpls = fsr["templates"];

            // Loop through templates
            for (auto &&t : tpls) {
                Template nTpl;
                t >> nTpl;
                templates.push_back(nTpl);
            }

            fsr.release();
            std::cout << " -> LOADED" << std::endl;
        }

        // Load data set
        cv::FileStorage fsr(trainedPath + "classifier.yml.gz", cv::FileStorage::READ);
        fsr["criteria"] >> criteria;
        std::cout << "  |_ info -> LOADED" << std::endl;
        std::cout << "  |_ loading hashtables..." << std::endl;

        // Load hash tables
        cv::FileNode hashTables = fsr["tables"];
        for (auto &&table : hashTables) {
            tables.emplace_back(HashTable::load(table, templates));
        }

        fsr.release();
        std::cout << "  |_ hashTables -> LOADED (" << tables.size() << ")" << std::endl;
        std::cout << "DONE!, took: " << tLoading.elapsed() << " s" << std::endl << std::endl;
    }

    void Classifier::detect(std::string trainedTemplatesListPath, std::string trainedPath, std::string scenePath) {
        // Init classifiers
        Parser parser(criteria);
        Objectness objectness(criteria);
        Hasher hasher(criteria);
        Matcher matcher(criteria);
        Visualizer viz(criteria);

        // Load trained template data
        load(trainedTemplatesListPath, trainedPath);

        // Image pyramid settings
        const float initialScale = .4f;
        const float scaleFactor = 1.25f;
        const int finalScaleLevel = 9;
        float scale = initialScale;

        // Timing
        Timer tTotal;
        double ttSceneLoading, ttObjectness, ttVerification, ttMatching, ttNMS;
        std::cout << "Matching started..." << std::endl << std::endl;

        for (int i = 0; i < 503; ++i) {
            ttSceneLoading = ttObjectness = ttVerification = ttMatching = ttNMS = 0;
            tTotal.reset();

            for (int pyramidLevel = 0; pyramidLevel < finalScaleLevel; ++pyramidLevel) {
                // Load scene
                Timer tSceneLoading;
                Scene scene = parser.parseScene(scenePath, i, scale);
                ttSceneLoading += tSceneLoading.elapsed();

                /// Objectness detection
                assert(criteria->info.smallestTemplate.area() > 0);
                assert(criteria->info.minEdgels > 0);

                Timer tObjectness;
                objectness.objectness(scene.srcDepth, windows);
                ttObjectness += tObjectness.elapsed();
//                viz.objectness(scene, windows);

                /// Verification and filtering of template candidates
                if (windows.empty()) {
                    continue;
                }

                Timer tVerification;
                hasher.verifyCandidates(scene.srcDepth, scene.srcNormals, tables, windows); // TODO refactor to use Scene as input
                ttVerification += tVerification.elapsed();
//                viz.windowsCandidates(scene, windows);

                /// Match templates
                Timer tMatching;
                matcher.match(scale, scene, windows, matches);
                ttMatching += tMatching.elapsed();

                scale *= scaleFactor;
                windows.clear();
            }

            std::cout << "Matches size: " << matches.size() << std::endl;

//            viz.preNonMaxima(scene, matches);

            // Apply non-maxima suppression
            Timer tNMS;
            nonMaximaSuppression(matches, criteria->overlapFactor);
            ttNMS = tNMS.elapsed();

            std::cout << std::endl << "Classification took: " << tTotal.elapsed() << "s" << std::endl;
            std::cout << "  |_ Scene loading took: " << ttSceneLoading << "s" << std::endl;
            std::cout << "  |_ Objectness detection took: " << ttObjectness << "s" << std::endl;
            std::cout << "  |_ Hashing verification took: " << ttVerification << "s" << std::endl;
            std::cout << "  |_ Template matching took: " << ttMatching << "s" << std::endl;
            std::cout << "  |_ NMS took: " << ttNMS << "s" << std::endl;

            // Vizualize results
            Scene scene = parser.parseScene(scenePath, i, 1.0f);
            viz.matches(scene, matches, 1);

            // Set scale to initial for next scene
            scale = initialScale;
            matches.clear();
        }
    }
}