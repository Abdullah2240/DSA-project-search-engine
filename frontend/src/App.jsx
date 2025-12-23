import SearchBar from './Components/SearchBar'
import SearchResults from './Components/SearchResults'
import PDFUpload from './Components/PDFUpload';
import { useState } from 'react';
import './App.css';
import { buildSearchUrl, API_ENDPOINTS } from './config/api';

function App() {
  const [results, setResults] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);
  const [searchStats, setSearchStats] = useState(null);
  const [hasSearched, setHasSearched] = useState(false);
  const [activeTab, setActiveTab] = useState('search'); // 'search' or 'upload'
  const [currentPage, setCurrentPage] = useState(1);

  const performSearch = async (query) => {
    setIsLoading(true);
    setError(null);
    setHasSearched(true);
    
    try {
      const apiUrl = buildSearchUrl(query);
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

  const handleUploadSuccess = (result) => {
    // No alert needed - PDFUpload handles the UI
    console.log('Upload successful:', result);
  };

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

        <div className="tabs">
          <button 
            className={`tab-button ${activeTab === 'search' ? 'active' : ''}`}
            onClick={() => setActiveTab('search')}
          >
            Search
          </button>
          <button 
            className={`tab-button ${activeTab === 'upload' ? 'active' : ''}`}
            onClick={() => setActiveTab('upload')}
          >
            Upload PDFs
          </button>
        </div>

        {activeTab === 'search' ? (
          <>
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

            {!isLoading && !error && hasSearched && results.length > 0 && (
              <SearchResults 
                results={results} 
                query={searchStats?.query}
                currentPage={currentPage}
                onPageChange={setCurrentPage}
                totalResults={searchStats?.total || results.length}
              />
            )}

            {!isLoading && !error && hasSearched && results.length === 0 && (
              <div className="empty-state">
                <div className="empty-state-icon">🔍</div>
                <h3>No results found</h3>
                <p>Try a different search query</p>
              </div>
            )}

            {!hasSearched && !isLoading && (
              <div className="empty-state">
                <div className="empty-state-icon">✨</div>
                <h3>Start Your Search</h3>
                <p>Enter a query above to explore the galaxy of documents</p>
              </div>
            )}
          </>
        ) : (
          <div className="upload-container">
            <PDFUpload onSuccess={handleUploadSuccess} />
          </div>
        )}
      </div>
    </>
  );
}

export default App
