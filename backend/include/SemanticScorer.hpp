#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstring>

/**
 * SemanticScorer: Handles semantic similarity using pre-trained word embeddings.
 * 
 * Uses GloVe embeddings (300-dim) to compute:
 * - Document vectors (average of word embeddings, normalized)
 * - Query vectors (average of query word embeddings)
 * - Cosine similarity between query and documents
 */
class SemanticScorer {
public:
    SemanticScorer();
    ~SemanticScorer();

    // Load document vectors and word embeddings from binary files
    bool load_document_vectors(const std::string& doc_vectors_path);
    bool load_word_embeddings(const std::string& word_embeddings_path);

    // Compute semantic similarity score (0.0 to 1.0)
    // Returns 0.0 if document or query not found
    double compute_similarity(int doc_id, const std::vector<std::string>& query_words) const;

    // Check if semantic scoring is available
    bool is_loaded() const { return vectors_loaded_ && embeddings_loaded_; }

    // Get number of loaded documents
    size_t num_documents() const { return document_vectors_.size(); }

private:
    static constexpr int EMBEDDING_DIM = 300;
    
    // Document vectors: doc_id -> 300-dim float vector
    std::unordered_map<int, std::vector<float>> document_vectors_;
    
    // Word embeddings: word -> 300-dim float vector
    std::unordered_map<std::string, std::vector<float>> word_embeddings_;

    bool vectors_loaded_;
    bool embeddings_loaded_;

    // Compute query vector from query words
    std::vector<float> compute_query_vector(const std::vector<std::string>& query_words) const;

    // Cosine similarity between two vectors
    double cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const;

    // Normalize vector to unit length
    void normalize_vector(std::vector<float>& vec) const;
};

