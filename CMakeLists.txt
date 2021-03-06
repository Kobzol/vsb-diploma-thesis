cmake_minimum_required(VERSION 3.6)
project(vsb-semestral-project)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -std=c++14 -march=native -Wall -pedantic")

set(SOURCE_FILES main.cpp utils/visualizer.h utils/visualizer.cpp core/template.cpp core/template.h objdetect/objectness.cpp objdetect/objectness.h utils/parser.cpp utils/parser.h utils/timer.h objdetect/hasher.cpp objdetect/hasher.h core/hash_key.cpp core/hash_key.h core/hash_table.cpp core/hash_table.h core/triplet.cpp core/triplet.h objdetect/classifier.cpp objdetect/classifier.h core/window.cpp core/window.h objdetect/matcher.cpp objdetect/matcher.h core/match.cpp core/match.h core/classifier_criteria.cpp core/classifier_criteria.h utils/timer.cpp processing/processing.cpp processing/processing.h processing/computation.h core/camera.cpp core/camera.h core/scene.cpp core/scene.h utils/converter.cpp utils/converter.h)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(Boost REQUIRED filesystem)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(vsb-semestral-project ${SOURCE_FILES})
target_link_libraries(vsb-semestral-project ${OpenCV_LIBRARIES} ${Boost_LIBRARIES})
