import './SearchResults.css'

function SearchResults({ results, query, currentPage, onPageChange, totalResults }) {
  const RESULTS_PER_PAGE = 10;
  const totalPages = Math.ceil(totalResults / RESULTS_PER_PAGE);
  const startIndex = (currentPage - 1) * RESULTS_PER_PAGE + 1;
  const endIndex = Math.min(currentPage * RESULTS_PER_PAGE, totalResults);

  const paginationNumbers = [];
  const maxVisiblePages = 10;
  
  let startPage = Math.max(1, currentPage - Math.floor(maxVisiblePages / 2));
  let endPage = Math.min(totalPages, startPage + maxVisiblePages - 1);
  
  if (endPage - startPage < maxVisiblePages - 1) {
    startPage = Math.max(1, endPage - maxVisiblePages + 1);
  }

  for (let i = startPage; i <= endPage; i++) {
    paginationNumbers.push(i);
  }

  const formatCitations = (count) => {
    if (!count || count === 0) return 'Not cited';
    if (count === 1) return '1 citation';
    if (count >= 1000) return `${(count / 1000).toFixed(1)}k citations`;
    return `${count.toLocaleString()} citations`;
  };

  return (
    <div className="search-results">
      <div className="results-header">
        <div className="results-stats">
          About {totalResults.toLocaleString()} results ({((currentPage - 1) * RESULTS_PER_PAGE + 1).toLocaleString()}-{endIndex.toLocaleString()})
        </div>
        <div className="results-time">(0.00 seconds)</div>
      </div>

      <div className="results-list">
        {results.map((result, idx) => (
          <div key={result.docId || idx} className="result-item">
            <div className="result-header">
              <a 
                href={result.url} 
                target="_blank" 
                rel="noopener noreferrer"
                className="result-title"
              >
                Document #{result.docId}
              </a>
              <span className="result-rank">#{idx + 1}</span>
            </div>

            <div className="result-metadata">
              <div className="metadata-item">
                <svg className="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/>
                  <polyline points="14 2 14 8 20 8"/>
                  <line x1="16" y1="13" x2="8" y2="13"/>
                  <line x1="16" y1="17" x2="8" y2="17"/>
                  <polyline points="10 9 9 9 8 9"/>
                </svg>
                <span className="metadata-value citation-count">
                  {formatCitations(result.cited_by_count)}
                </span>
              </div>

              {result.score && (
                <div className="metadata-item">
                  <svg className="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                    <polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/>
                  </svg>
                  <span className="metadata-value">
                    Score: {result.score.toFixed(2)}
                  </span>
                </div>
              )}
            </div>

            <p className="result-url">
              <svg className="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/>
                <path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/>
              </svg>
              <a 
                href={result.url} 
                target="_blank" 
                rel="noopener noreferrer"
                className="url-link"
              >
                {result.url}
              </a>
            </p>
          </div>
        ))}
      </div>

      {totalPages > 1 && (
        <div className="pagination">
          {currentPage > 1 && (
            <button 
              className="pagination-btn prev"
              onClick={() => onPageChange(currentPage - 1)}
            >
              <span>‹</span> Previous
            </button>
          )}
          
          <div className="pagination-numbers">
            {startPage > 1 && (
              <>
                <button 
                  className="pagination-number"
                  onClick={() => onPageChange(1)}
                >
                  1
                </button>
                {startPage > 2 && <span className="pagination-ellipsis">...</span>}
              </>
            )}
            
            {paginationNumbers.map(num => (
              <button
                key={num}
                className={`pagination-number ${num === currentPage ? 'active' : ''}`}
                onClick={() => onPageChange(num)}
              >
                {num}
              </button>
            ))}
            
            {endPage < totalPages && (
              <>
                {endPage < totalPages - 1 && <span className="pagination-ellipsis">...</span>}
                <button 
                  className="pagination-number"
                  onClick={() => onPageChange(totalPages)}
                >
                  {totalPages}
                </button>
              </>
            )}
          </div>

          {currentPage < totalPages && (
            <button 
              className="pagination-btn next"
              onClick={() => onPageChange(currentPage + 1)}
            >
              Next <span>›</span>
            </button>
          )}
        </div>
      )}
    </div>
  );
}

export default SearchResults;

