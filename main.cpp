#include "eval-simd.h"
#include "eval.h"
#include "model.h"
#include "timing.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

std::unique_ptr<float[]> read_test_data(std::string filename, int num_rows, int num_cols, const float missing_val)
{
  std::ifstream infile(filename.c_str(), std::ios_base::in);
  std::unique_ptr<float[]> values = std::make_unique<float[]>(num_rows * num_cols);

  std::string buffer;
  if (infile) {
    for (int i = 0; i < num_rows; i++) {
      float* d = &values[i * num_cols];
      for (int j = 0; j < num_cols; j++) {
        float f;
        infile >> f;
        if (f == missing_val) {
          f = std::numeric_limits<float>::quiet_NaN();
        }
        if (j != num_cols - 1) {
          std::getline(infile, buffer, ',');
        }
        d[j] = f;
      }
      std::getline(infile, buffer);
    }
  }
  infile.close();
  return values;
}

int main(int argc, char** argv)
{
  if (argc <= 1) {
    std::cout << "Usage: ./fast-tree {breadth-first | preorder | preorder-cover | treelite | simd}" << std::endl;
    exit(1);
  }
  const int NUM_ROWS = 550000;
  const int NUM_COLS = 30;
  const float MISSING_VAL = -999.0;
  const std::string TEST_FILENAME = "../higgs-boson/data/test_raw.csv";
  const std::string MODEL_FILENAME = "../higgs-boson/higgs-model-single-depth-3.txt";
  const std::string BENCHMARK = argv[1];

  std::cout << "Benchmark: " << BENCHMARK << std::endl;
  std::unique_ptr<float[]> test_inputs = read_test_data(TEST_FILENAME, NUM_ROWS, NUM_COLS, MISSING_VAL);
  std::ofstream predictions_outfile;
  const std::string predictions_fname = "predictions.csv";
  predictions_outfile.open(predictions_fname);

  if (BENCHMARK == "breadth-first") {
    std::vector<node_t> model = read_model_breadth_first(MODEL_FILENAME);
    bench_timer_t start = time_start();
    for (int i = 0; i < NUM_ROWS * NUM_COLS; i += NUM_COLS) {
      float prediction = evaluate_tree_regression_yelp_breadth_first(model, &test_inputs[i]);
      predictions_outfile << std::fixed << std::setprecision(17) << prediction << std::endl;
      // if (i % 1000 == 0) {
      //   std::cout << "Prediction " << i / NUM_COLS << ": " << std::fixed << std::setprecision(17) << prediction << std::endl;
      // }
    }
    const double time = time_stop(start);
    std::cout << "Total prediction time: " << time << "s" << std::endl;

  } else if (BENCHMARK == "preorder") {
    std::vector<node_t> model = read_model_preorder(MODEL_FILENAME);
    bench_timer_t start = time_start();
    for (int i = 0; i < NUM_ROWS * NUM_COLS; i += NUM_COLS) {
      float prediction = evaluate_tree_regression_yelp_preorder(model, &test_inputs[i]);
      predictions_outfile << std::fixed << std::setprecision(17) << prediction << std::endl;
      // if (i % 1000 == 0) {
      //   std::cout << "Prediction " << i / NUM_COLS << ": " << std::fixed << std::setprecision(17) << prediction << std::endl;
      // }
    }
    const double time = time_stop(start);
    std::cout << "Total prediction time: " << time << "s" << std::endl;

  } else if (BENCHMARK == "preorder-cover") {
    std::vector<node_t> model = read_model_preorder(MODEL_FILENAME, true);
    bench_timer_t start = time_start();
    for (int i = 0; i < NUM_ROWS * NUM_COLS; i += NUM_COLS) {
      float prediction = evaluate_tree_regression_yelp_preorder_cover(model, &test_inputs[i]);
      predictions_outfile << std::fixed << std::setprecision(17) << prediction << std::endl;
      // if (i % 1000 == 0) {
      //   std::cout << "Prediction " << i / NUM_COLS << ": " << std::fixed << std::setprecision(17) << prediction << std::endl;
      // }
    }
    const double time = time_stop(start);
    std::cout << "Total prediction time: " << time << "s" << std::endl;

  } else if (BENCHMARK == "treelite") {
    bench_timer_t start = time_start();
    for (int i = 0; i < NUM_ROWS * NUM_COLS; i += NUM_COLS) {
      float prediction = evaluate_tree_regression_treelite(&test_inputs[i]);
      predictions_outfile << std::fixed << std::setprecision(17) << prediction << std::endl;
      // if (i % 1000 == 0) {
      //   std::cout << "Prediction " << i / NUM_COLS << ": " << std::fixed << std::setprecision(17) << prediction << std::endl;
      // }
    }
    const double time = time_stop(start);
    std::cout << "Total prediction time: " << time << "s" << std::endl;

  } else if (BENCHMARK == "simd") {
    std::vector<node_t> model = read_model_breadth_first(MODEL_FILENAME);
    float lookup_table[128];
    for (int i = 0; i < 128; ++i) {
      if ((i & 11) == 11)
        lookup_table[i] = -0.0825883001;
      else if ((i & 3) == 3)
        lookup_table[i] = 0.063217625;
      else if ((i & 17) == 17)
        lookup_table[i] = 0.138557851;
      else if ((i & 1) == 1)
        lookup_table[i] = -0.160050601;
      else if ((i & 36) == 36)
        lookup_table[i] = -0.09623261541;
      else if ((i & 4) == 4)
        lookup_table[i] = 0.0580137558;
      else if ((i & 64) == 64)
        lookup_table[i] = -0.183263466;
      else
        // mask & 0 == 0
        lookup_table[i] = -0.0119630694;
    }
    float split_values[8];
    for (int i = 0; i < 7; ++i) {
    split_values[i] = model[i].split_value;
    }
    split_values[7] = 0.0;

    bench_timer_t start = time_start();
    for (int i = 0; i < NUM_ROWS * NUM_COLS; i += NUM_COLS) {
      float prediction = evaluate_tree_simd(model, split_values, lookup_table, &test_inputs[i]);
      predictions_outfile << std::fixed << std::setprecision(17) << prediction << std::endl;
      // if (i % 1000 == 0) {
      //   std::cout << "Prediction " << i / NUM_COLS << ": " << std::fixed << std::setprecision(17) << prediction << std::endl;
      // }
    }
    const double time = time_stop(start);
    std::cout << "Total prediction time: " << time << "s" << std::endl;
  } else {
    std::cout << "Not a valid benchmark" << std::endl;
    exit(1);
  }

  predictions_outfile.close();
  for (const auto& key_value : NODE_COUNTS) {
    std::cout << "Node " << key_value.first << " has true cover " << key_value.second << std::endl;
  }
  return 0;
}
