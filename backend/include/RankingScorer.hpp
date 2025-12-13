#pragma once
// RankingScorer.hpp
// Multi-factor ranking system based on frequency, position, title, and metadata
// Supports configurable weightages for different ranking factors

#include <string>
#include <vector>
#include <cmath>
#include "DocumentMetadata.hpp"

// Structure to hold scoring components for a document
struct ScoreComponents {
    double frequency_score;
    double position_score;
    double title_boost;
    double metadata_score;
    double date_boost;
    double final_score;
    
    ScoreComponents() : frequency_score(0), position_score(0), title_boost(1.0), 
                        metadata_score(0), date_boost(1.0), final_score(0) {}
};

class RankingScorer {
public:
    RankingScorer();
    
    // Calculate final score for a document
    // Parameters:
    //   - weighted_frequency: title_freq * 3 + body_freq
    //   - title_frequency: frequency in title (0 if not in title)
    //   - positions: vector of positions where word appears
    //   - doc_id: document ID for metadata lookup
    //   - metadata: pointer to DocumentMetadata (can be nullptr)
    ScoreComponents calculate_score(
        int weighted_frequency,
        int title_frequency,
        const std::vector<int>& positions,
        int doc_id,
        const DocumentMetadata* metadata = nullptr
    ) const;
    
    // Configure weights (optional - uses defaults if not called)
    void set_weights(double freq_weight, double pos_weight, double title_weight, double meta_weight);
    
    // Get current weights
    void get_weights(double& freq_weight, double& pos_weight, double& title_weight, double& meta_weight) const;

private:
    // Weight configuration
    double weight_frequency_;  // Weight for frequency component (default: 0.4)
    double weight_position_;    // Weight for position component (default: 0.2)
    double weight_title_;      // Weight for title boost (default: 0.3)
    double weight_metadata_;   // Weight for metadata component (default: 0.1)
    
    // Helper methods
    double calculate_frequency_score(int weighted_frequency) const;
    double calculate_position_score(const std::vector<int>& positions) const;
    double calculate_title_boost(int title_frequency) const;
    double calculate_metadata_score(int doc_id, const DocumentMetadata* metadata) const;
    double calculate_date_boost(int publication_year) const;
};

