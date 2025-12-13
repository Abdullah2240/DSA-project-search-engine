#include "RankingScorer.hpp"
#include <algorithm>
#include <cmath>

RankingScorer::RankingScorer() 
    : weight_frequency_(0.4), 
      weight_position_(0.2), 
      weight_title_(0.3), 
      weight_metadata_(0.1) {
}

void RankingScorer::set_weights(double freq_weight, double pos_weight, double title_weight, double meta_weight) {
    weight_frequency_ = freq_weight;
    weight_position_ = pos_weight;
    weight_title_ = title_weight;
    weight_metadata_ = meta_weight;
}

void RankingScorer::get_weights(double& freq_weight, double& pos_weight, double& title_weight, double& meta_weight) const {
    freq_weight = weight_frequency_;
    pos_weight = weight_position_;
    title_weight = weight_title_;
    meta_weight = weight_metadata_;
}

ScoreComponents RankingScorer::calculate_score(
    int weighted_frequency,
    int title_frequency,
    const std::vector<int>& positions,
    int doc_id,
    const DocumentMetadata* metadata
) const {
    ScoreComponents scores;
    
    // Component 1: Frequency Score (logarithmic to prevent dominance)
    scores.frequency_score = calculate_frequency_score(weighted_frequency);
    
    // Component 2: Position Score (earlier positions = higher score)
    scores.position_score = calculate_position_score(positions);
    
    // Component 3: Title Boost (documents with query in title get boost)
    scores.title_boost = calculate_title_boost(title_frequency);
    
    // Component 4: Metadata Score (citations, keywords, etc.)
    scores.metadata_score = calculate_metadata_score(doc_id, metadata);
    
    // Component 5: Date Boost (recent documents get slight boost)
    int publication_year = 0;
    if (metadata) {
        publication_year = metadata->get_publication_year(doc_id);
    }
    scores.date_boost = calculate_date_boost(publication_year);
    
    // Calculate final weighted score
    scores.final_score = (
        scores.frequency_score * weight_frequency_ +
        scores.position_score * weight_position_ +
        scores.title_boost * weight_title_ +
        scores.metadata_score * weight_metadata_
    ) * scores.date_boost;
    
    return scores;
}

double RankingScorer::calculate_frequency_score(int weighted_frequency) const {
    // Logarithmic scaling to prevent high-frequency words from dominating
    // log(1 + freq) gives diminishing returns
    return std::log1p(static_cast<double>(weighted_frequency));
}

double RankingScorer::calculate_position_score(const std::vector<int>& positions) const {
    if (positions.empty()) return 0.0;
    
    double score = 0.0;
    // First 10 positions get higher weight
    // Position 0 gets 1.0, position 9 gets 0.1
    for (int pos : positions) {
        if (pos < 10) {
            score += (10.0 - static_cast<double>(pos)) * 0.1;
        } else if (pos < 50) {
            // Positions 10-49 get smaller weight
            score += (50.0 - static_cast<double>(pos)) * 0.01;
        }
        // Positions beyond 50 don't contribute to position score
    }
    
    // Normalize by number of positions (average)
    return score / std::max(1.0, static_cast<double>(positions.size()));
}

double RankingScorer::calculate_title_boost(int title_frequency) const {
    // If word appears in title, give 2x boost
    // Otherwise, neutral (1.0)
    return (title_frequency > 0) ? 2.0 : 1.0;
}

double RankingScorer::calculate_metadata_score(int doc_id, const DocumentMetadata* metadata) const {
    if (!metadata) return 0.0;
    
    double score = 0.0;
    
    // Citation-based score (logarithmic to handle large citation counts)
    int cited_count = metadata->get_cited_by_count(doc_id);
    if (cited_count > 0) {
        // log(citations) * 0.3 gives diminishing returns
        score += std::log1p(static_cast<double>(cited_count)) * 0.3;
    }

    return score;
}

double RankingScorer::calculate_date_boost(int publication_year) const {
    if (publication_year <= 0) {
        return 1.0;  // No boost if year unknown
    }
    
    // Recent documents get slight boost
    // Formula: 1.0 + (year - 2000) * 0.01
    // Year 2024 gets: 1.0 + 24 * 0.01 = 1.24x boost
    // Year 2000 gets: 1.0 + 0 * 0.01 = 1.0x (neutral)
    // Year 1990 gets: 1.0 + (-10) * 0.01 = 0.9x (slight penalty)
    
    double boost = 1.0 + (publication_year - 2000) * 0.01;
    
    // Clamp between 0.5 and 2.0 to prevent extreme values
    return std::max(0.5, std::min(2.0, boost));
}

