#include "lexicon.hpp"
#include <cctype>
#include <algorithm>
#include <regex>
#include <iterator>
#include <stdexcept>

#include "json.hpp"
using json = nlohmann::json;

using namespace std;
namespace fs = std::filesystem;

// Constructor that willl load default stopwords
Lexicon::Lexicon() {
    load_default_stopwords();
}

// Configuration setters
void Lexicon::set_min_frequency(int freq) {
    min_frequency_ = max(1, freq);
}

void Lexicon::set_max_frequency_percentile(int percentile) {
    max_frequency_percentile_ = clamp(percentile, 1, 100);
}

void Lexicon::set_stopwords_path(const string& path) {
    stopwords_path_ = path;
    try {
        load_stopwords_from_file(path);
    } catch (const std::exception &e) {
        cerr << "Warning: Could not load stopwords from file: " << e.what() << "\n";
    }
}

// Load default stopwords
void Lexicon::load_default_stopwords() {
    static const char* defaults[] = {
        "the","a","an","and","or","but","in","on","at","to","for","of","with","by","from",
        "as","is","was","are","were","be","have","has","had","do","does","did","will","would",
        "should","could","may","might","must","can","this","that","these","those","i","you",
        "he","she","it","we","they","what","which","who","when","where","why","how","all",
        "each","every","both","few","more","most","other","some","such","no","not","only",
        "own","same","so","than","too","very","now","then","there","their","them","through",
        "under","until","up","use","using","via","year","years","your","yours"
    };
    stop_words_.clear();
    for (const auto &w : defaults) stop_words_.insert(string(w));
}

// Load stopwords from file
void Lexicon::load_stopwords_from_file(const string& path) {
    ifstream in(path);
    if (!in.is_open()) throw runtime_error("Stopwords file not found: " + path);

    stop_words_.clear();
    string line;
    while (getline(in, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        string tok = line.substr(start, end - start + 1);
        transform(tok.begin(), tok.end(), tok.begin(), [](unsigned char c){ return tolower(c); });
        if (!tok.empty()) stop_words_.insert(tok);
    }
}

// Check if a token is significant (i.e.not stopword, length >=3 and not all digits)
bool Lexicon::is_significant_word(const string& word) const {
    if (word.empty()) return false;

    string w = word;
    transform(w.begin(), w.end(), w.begin(), [](unsigned char c){ return tolower(c); });

    if (stop_words_.count(w)) return false;
    if (w.size() < 3) return false;
    if (all_of(w.begin(), w.end(), [](unsigned char c){ return isdigit(c); })) return false;

    return true;
}

// Extract tokens from a JSONL line
vector<string> Lexicon::parse_tokens_from_jsonl_line(const string& json_line) const {
    vector<string> tokens;
    try {
        json j = json::parse(json_line);
        if (!j.contains("tokens") || !j["tokens"].is_array()) return tokens;
        for (const auto &item : j["tokens"]) {
            if (item.is_string()) {
                string s = item.get<string>();
                if (!s.empty()) tokens.push_back(s);
            }
        }
    } catch (const json::parse_error &) {
        // ignore malformed line
    }
    return tokens;
}

// Escape string for JSON
string Lexicon::json_escape(const string& str) const {
    string result;
    result.reserve(str.size() * 2);
    char buf[16];

    for (unsigned char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 32) { snprintf(buf, sizeof(buf), "\\u%04x", c); result += buf; }
                else result += c;
        }
    }

    return result;
}

// Build lexicon from cleaned 
bool Lexicon::build_from_jsonl(const string& cleaned_data_path, const string& output_path) {
    word_to_index_.clear();
    index_to_word_.clear();
    unordered_map<string,int> word_frequencies;
    word_frequencies.reserve(65536);

    int total_documents = 0;
    ifstream in(cleaned_data_path);
    if (!in.is_open()) {
        cerr << "Error: could not open " << cleaned_data_path << endl;
        return false;
    }

    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        vector<string> tokens = parse_tokens_from_jsonl_line(line);
        unordered_set<string> doc_set;

        for (string tok : tokens) {
            transform(tok.begin(), tok.end(), tok.begin(), [](unsigned char c){ return tolower(c); });
            if (doc_set.insert(tok).second) word_frequencies[tok]++;
        }
        total_documents++;
    }
    in.close();

    cout << "Total documents: " << total_documents << "\n";
    cout << "Unique tokens: " << word_frequencies.size() << "\n";

    // Compute frequency list (ascending)
    vector<int> freqs;
    freqs.reserve(word_frequencies.size());
    for (auto &p : word_frequencies) freqs.push_back(p.second);
    sort(freqs.begin(), freqs.end());

    size_t n = freqs.size();

    // Determine upper cutoff (exclude most frequent words) based on max_frequency_percentile_
    int max_freq = INT_MAX;
    if (n > 0 && max_frequency_percentile_ < 100) {
        // keep_count = number of entries to keep (based on percentile)
        size_t keep_count = (n * static_cast<size_t>(max_frequency_percentile_)) / 100;
        if (keep_count == 0) keep_count = 1;               // keep at least one
        if (keep_count > n) keep_count = n;
        size_t threshold_index = keep_count - 1;           // last index to keep
        int cutoff = freqs[threshold_index];
        // we keep p.second <= cutoff, so set max_freq = cutoff + 1 and use p.second < max_freq below
        max_freq = cutoff + 1;
    }

    // Collect significant words, skipping only very-high-frequency words
    vector<pair<string,int>> significant;
    for (auto &p : word_frequencies) {
        int freq = p.second;
        if (!is_significant_word(p.first)) continue;
        if (freq < min_frequency_) continue;
        if (max_freq != INT_MAX && !(freq < max_freq)) continue;         // exclude top-frequency words
        significant.emplace_back(p.first, freq);
    }

    sort(significant.begin(), significant.end(), [](const auto &a, const auto &b){ return a.first < b.first; });
    index_to_word_.reserve(significant.size());
    for (size_t i = 0; i < significant.size(); ++i) {
        word_to_index_[significant[i].first] = static_cast<int>(i);
        index_to_word_.push_back(significant[i].first);
    }

    cout << "Significant words: " << index_to_word_.size() << "\n";

    // Ensure output directory exists
    try {
        fs::path outp(output_path);
        if (outp.has_parent_path()) fs::create_directories(outp.parent_path());
    } catch (const exception &e) {
        cerr << "Warning: could not create directories: " << e.what() << "\n";
    }

    return save_to_json(output_path);
}

// Save lexicon to JSON
bool Lexicon::save_to_json(const string& output_path) const {
    json j;
    j["word_to_index"] = json::object();
    for (const auto &p : word_to_index_) j["word_to_index"][p.first] = p.second;
    j["index_to_word"] = index_to_word_;
    j["total_words"] = index_to_word_.size();

    ofstream out(output_path);
    if (!out.is_open()) {
        cerr << "Error: could not create file " << output_path << "\n";
        return false;
    }

    out << j.dump(4) << "\n";
    cout << "Saved lexicon to " << output_path << "\n";
    return true;
}

// Load lexicon from JSON
bool Lexicon::load_from_json(const string& lexicon_path) {
    ifstream in(lexicon_path);
    if (!in.is_open()) {
        cerr << "Error: could not open " << lexicon_path << "\n";
        return false;
    }

    json j;
    try {
        in >> j;
    } catch (const json::parse_error &e) {
        cerr << "Error parsing JSON: " << e.what() << "\n";
        return false;
    }

    word_to_index_.clear();
    index_to_word_.clear();

    if (j.contains("word_to_index") && j["word_to_index"].is_object()) {
        for (auto it = j["word_to_index"].begin(); it != j["word_to_index"].end(); ++it) {
            if (it.value().is_number_integer())
                word_to_index_.emplace(it.key(), it.value().get<int>());
        }
    }

    if (j.contains("index_to_word") && j["index_to_word"].is_array()) {
        index_to_word_ = j["index_to_word"].get<vector<string>>();
        if (word_to_index_.empty()) {
            for (size_t i = 0; i < index_to_word_.size(); ++i)
                word_to_index_[index_to_word_[i]] = static_cast<int>(i);
        }
    }

    cout << "Loaded lexicon: " << word_to_index_.size() << " entries\n";
    return !word_to_index_.empty();
}

// Lookup helpers
int Lexicon::get_word_index(const string& word) const {
    string w = word;
    transform(w.begin(), w.end(), w.begin(), [](unsigned char c){ return tolower(c); });
    auto it = word_to_index_.find(w);
    return (it == word_to_index_.end()) ? -1 : it->second;
}

string Lexicon::get_word(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= index_to_word_.size()) return "";
    return index_to_word_[index];
}

size_t Lexicon::size() const { return word_to_index_.size(); }
bool Lexicon::contains_word(const string& word) const { return get_word_index(word) != -1; }
