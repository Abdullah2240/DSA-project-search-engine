#include "lexicon.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>

using namespace std;

int main(int argc, char* argv[]) {
    // Try cleaned_with_body.jsonl first (from crawl_pdf.py), fallback to cleaned.jsonl
    string input_path = "data/processed/test.jsonl";
    ifstream test_file(input_path);
    if (!test_file.good()) {
        input_path = "data/processed/test.jsonl";
    }
    test_file.close();
    string output_path = "data/processed/lexicon.json";
    
    if (argc >= 2) input_path = argv[1];
    if (argc >= 3) output_path = argv[2];
    
    cout << "Building lexicon from: " << input_path << "\n";
    cout << "Output: " << output_path << "\n\n";
    
    Lexicon lexicon;
    lexicon.set_min_frequency(1);
    lexicon.set_max_frequency_percentile(100);
    
    if (!lexicon.build_from_jsonl(input_path, output_path)) {
        cerr << "Error: Failed to build lexicon\n";
        return 1;
    }
    
    cout << "\nLexicon built successfully!\n";
    cout << "Total words: " << lexicon.size() << "\n";
    
    Lexicon test;
    if (test.load_from_json(output_path) && test.size() == lexicon.size()) {
        cout << "Verification: Lexicon loads correctly\n";
        cout << "\nFirst 10 words:\n";
        for (size_t i = 0; i < min((size_t)10, test.size()); ++i) {
            cout << "  [" << i << "] " << test.get_word(static_cast<int>(i)) << "\n";
        }
    }
    
    return 0;
}


