#include "../include/SemanticScorer.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>

SemanticScorer::SemanticScorer() : vectors_loaded_(false), embeddings_loaded_(false) {}

SemanticScorer::~SemanticScorer() {}

bool SemanticScorer::load_document_vectors(const std::string& doc_vectors_path) {
    std::ifstream file(doc_vectors_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SemanticScorer] Could not open document vectors file: " << doc_vectors_path << "\n";
        return false;
    }

    int num_docs;
    file.read(reinterpret_cast<char*>(&num_docs), sizeof(int));

    document_vectors_.clear();
    document_vectors_.reserve(num_docs);

    for (int i = 0; i < num_docs; ++i) {
        int doc_id;
        file.read(reinterpret_cast<char*>(&doc_id), sizeof(int));

        std::vector<float> vector(EMBEDDING_DIM);
        file.read(reinterpret_cast<char*>(vector.data()), EMBEDDING_DIM * sizeof(float));

        if (file.good()) {
            document_vectors_[doc_id] = std::move(vector);
        } else {
            break;
        }
    }

    vectors_loaded_ = (document_vectors_.size() > 0);
    if (vectors_loaded_) {
        std::cout << "[SemanticScorer] Loaded " << document_vectors_.size() << " document vectors\n";
    }
    return vectors_loaded_;
}

bool SemanticScorer::load_word_embeddings(const std::string& word_embeddings_path) {
    std::ifstream file(word_embeddings_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SemanticScorer] Could not open word embeddings file: " << word_embeddings_path << "\n";
        return false;
    }

    int num_words;
    file.read(reinterpret_cast<char*>(&num_words), sizeof(int));

    word_embeddings_.clear();
    word_embeddings_.reserve(num_words);

    for (int i = 0; i < num_words; ++i) {
        int word_len;
        file.read(reinterpret_cast<char*>(&word_len), sizeof(int));

        std::string word(word_len, '\0');
        file.read(&word[0], word_len);

        std::vector<float> vector(EMBEDDING_DIM);
        file.read(reinterpret_cast<char*>(vector.data()), EMBEDDING_DIM * sizeof(float));

        if (file.good()) {
            // Normalize word embedding
            normalize_vector(vector);
            word_embeddings_[word] = std::move(vector);
        } else {
            break;
        }
    }

    embeddings_loaded_ = (word_embeddings_.size() > 0);
    if (embeddings_loaded_) {
        std::cout << "[SemanticScorer] Loaded " << word_embeddings_.size() << " word embeddings\n";
    }
    return embeddings_loaded_;
}

std::vector<float> SemanticScorer::compute_query_vector(const std::vector<std::string>& query_words) const {
    std::vector<float> query_vec(EMBEDDING_DIM, 0.0f);
    int valid_words = 0;

    for (const auto& word : query_words) {
        auto it = word_embeddings_.find(word);
        if (it != word_embeddings_.end()) {
            const auto& word_vec = it->second;
            for (int i = 0; i < EMBEDDING_DIM; ++i) {
                query_vec[i] += word_vec[i];
            }
            valid_words++;
        }
    }

    if (valid_words == 0) {
        return std::vector<float>(EMBEDDING_DIM, 0.0f);
    }

    // Average
    for (int i = 0; i < EMBEDDING_DIM; ++i) {
        query_vec[i] /= valid_words;
    }

    // Normalize
    normalize_vector(query_vec);
    return query_vec;
}

double SemanticScorer::compute_similarity(int doc_id, const std::vector<std::string>& query_words) const {
    if (!is_loaded()) {
        return 0.0;
    }

    auto doc_it = document_vectors_.find(doc_id);
    if (doc_it == document_vectors_.end()) {
        return 0.0;
    }

    std::vector<float> query_vec = compute_query_vector(query_words);
    if (query_vec.empty() || std::all_of(query_vec.begin(), query_vec.end(), [](float x) { return x == 0.0f; })) {
        return 0.0;
    }

    return cosine_similarity(query_vec, doc_it->second);
}

double SemanticScorer::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.size() != EMBEDDING_DIM) {
        return 0.0;
    }

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (int i = 0; i < EMBEDDING_DIM; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    if (norm_a == 0.0 || norm_b == 0.0) {
        return 0.0;
    }

    double similarity = dot_product / (norm_a * norm_b);
    // Clamp to [0, 1] (cosine similarity is [-1, 1], but normalized vectors give [0, 1])
    return std::max(0.0, std::min(1.0, similarity));
}

void SemanticScorer::normalize_vector(std::vector<float>& vec) const {
    double norm = 0.0;
    for (float x : vec) {
        norm += x * x;
    }
    norm = std::sqrt(norm);

    if (norm > 0.0) {
        for (float& x : vec) {
            x /= norm;
        }
    }
}

