import { useState, useEffect } from 'react'
import './SlidingCard.css'

function SlidingCard({ isVisible, onClose }) {
  const [isAnimating, setIsAnimating] = useState(false);

  useEffect(() => {
    if (isVisible) {
      setIsAnimating(true);
    }
  }, [isVisible]);

  if (!isVisible && !isAnimating) return null;

  return (
    <div 
      className={`sliding-card ${isVisible ? 'visible' : 'hidden'}`}
      onTransitionEnd={() => {
        if (!isVisible) setIsAnimating(false);
      }}
    >
      <div className="sliding-card-content">
        <button className="close-btn" onClick={onClose}>×</button>
        <div className="card-header">
          <h2>About Semantic Search</h2>
        </div>
        <div className="card-body">
          <p>
            This search engine uses <strong>hybrid search</strong> combining:
          </p>
          <ul>
            <li><strong>60% Keyword Search</strong> - Traditional TF-IDF matching</li>
            <li><strong>40% Semantic Search</strong> - AI-powered understanding using GloVe embeddings</li>
          </ul>
          <p>
            This means the search understands <em>concepts</em> and <em>synonyms</em>, 
            not just exact word matches.
          </p>
          <div className="example-section">
            <h3>Example:</h3>
            <p>
              Searching for <strong>"machine learning"</strong> will also find documents 
              about <em>"neural networks"</em>, <em>"artificial intelligence"</em>, 
              and <em>"deep learning"</em>.
            </p>
          </div>
        </div>
      </div>
    </div>
  );
}

export default SlidingCard;

