import SearchBar from './Components/SearchBar'
import {useState} from 'react'
import './App.css'

function App() {
  const [results, setResults] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);
  const [searchStats, setSearchStats] = useState(null);
  const [hasSearched, setHasSearched] = useState(false);

  const performSearch = async (query) => {
    setIsLoading(true);
    setError(null);
    setHasSearched(true);
    
    try {
      const apiUrl = `http://localhost:8080/search?q=${encodeURIComponent(query)}`;
      const res = await fetch(apiUrl);
      
      if (!res.ok) {
        throw new Error(`Search failed: ${res.status} ${res.statusText}`);
      }
      
      const data = await res.json();
      setResults(data.results || []);
      setSearchStats({
        total: data.results?.length || 0,
        query: data.query || query
      });
    } catch (err) {
      console.error("Error while searching", err);
      setError(err.message || "Failed to perform search. Make sure the backend server is running.");
      setResults([]);
      setSearchStats(null);
    } finally {
      setIsLoading(false);
    }
  }

  // Generate stars for background
  const generateStars = () => {
    const stars = [];
    for (let i = 0; i < 10; i++) {
      stars.push(<div key={i} className="star"></div>);
    }
    return stars;
  }

  return (
    <>
      <div className="galaxy-container">
        <div className="stars">
          {generateStars()}
        </div>
        <div className="nebula nebula-1"></div>
        <div className="nebula nebula-2"></div>
        <div className="nebula nebula-3"></div>
      </div>

      <div className="app-container">
        <div className="header">
          <h1>Galaxy Search</h1>
          <p>Explore the cosmos of knowledge</p>
        </div>

        <div className="search-container">
          <SearchBar onSearch={performSearch} isLoading={isLoading} />
        </div>

        {isLoading && (
          <div className="loading-container">
            <div>
              <div className="galaxy-loader"></div>
              <p className="loader-text">Searching the galaxy...</p>
            </div>
          </div>
        )}

        {error && (
          <div className="error-state">
            <h3>Search Error</h3>
            <p>{error}</p>
          </div>
        )}

        {!isLoading && !error && hasSearched && (
          <div className="results-container">
            {searchStats && (
              <div className="stats">
                <div className="stat-item">
                  <span>Results:</span>
                  <span className="stat-value">{searchStats.total.toLocaleString()}</span>
                </div>
                <div className="stat-item">
                  <span>Query:</span>
                  <span className="stat-value">"{searchStats.query}"</span>
                </div>
              </div>
            )}

            {results.length > 0 ? (
              <div className="results-list">
                {results.map((result, idx) => (
                  <div 
                    key={result.docId || idx} 
                    className="result-item"
                    style={{ animationDelay: `${idx * 0.05}s` }}
                  >
                    <div className="result-header">
                      <span className="result-doc-id">Document #{result.docId}</span>
                      <div className="result-score">
                        <span>Score:</span>
                        <span className="score-badge">{result.score}</span>
                      </div>
                    </div>
                    {result.url && (
                      <a 
                        href={result.url} 
                        target="_blank" 
                        rel="noopener noreferrer"
                        className="result-url"
                      >
                        {result.url}
                      </a>
                    )}
                  </div>
                ))}
              </div>
            ) : hasSearched && !isLoading ? (
              <div className="empty-state">
                <div className="empty-state-icon">🔍</div>
                <h3>No results found</h3>
                <p>Try a different search query</p>
              </div>
            ) : null}
          </div>
        )}

        {!hasSearched && !isLoading && (
          <div className="empty-state">
            <div className="empty-state-icon">✨</div>
            <h3>Start Your Search</h3>
            <p>Enter a query above to explore the galaxy of documents</p>
          </div>
        )}
      </div>
    </>
  );
}

export default App
