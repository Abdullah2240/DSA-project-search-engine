#pragma once
#include <string>
#include <vector>
#include "lexicon.hpp"

using namespace std;
// Integrating lexicon functionality
vector<string> search(const string query);
vector<int> tokenize_query(const string query, Lexicon& lexicon);
vector<string> get_lexicon_stats();