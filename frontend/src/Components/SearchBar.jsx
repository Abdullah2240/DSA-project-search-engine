import { useState } from "react"
import './SearchBar.css'

export default function SearchBar({ onSearch, isLoading }) {
    const [query, setQuery] = useState("")

    const handleSubmit = e => {
        e.preventDefault()
        const trimmedQuery = query.trim()
        if (trimmedQuery !== "") {
            onSearch(trimmedQuery)
        }
    }

    const handleKeyDown = (e) => {
        if (e.key === 'Enter') {
            handleSubmit(e)
        }
    }

    return (
        <div className="search-bar-wrapper">
            <form onSubmit={handleSubmit} className="search-form">
                <div className="search-input-container">
                    <svg 
                        className="search-icon" 
                        width="20" 
                        height="20" 
                        viewBox="0 0 24 24" 
                        fill="none" 
                        stroke="currentColor" 
                        strokeWidth="2"
                    >
                        <circle cx="11" cy="11" r="8"></circle>
                        <path d="m21 21-4.35-4.35"></path>
                    </svg>
                    <input 
                        type="text" 
                        className="search-input"  
                        value={query} 
                        onChange={e => setQuery(e.target.value)}
                        onKeyDown={handleKeyDown}
                        placeholder="Search the cosmos..." 
                        disabled={isLoading}
                        autoFocus
                    />
                    {isLoading && (
                        <div className="search-loading">
                            <div className="mini-spinner"></div>
                        </div>
                    )}
                    {query && !isLoading && (
                        <button 
                            type="button"
                            className="clear-button"
                            onClick={() => setQuery("")}
                            aria-label="Clear search"
                        >
                            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                                <line x1="18" y1="6" x2="6" y2="18"></line>
                                <line x1="6" y1="6" x2="18" y2="18"></line>
                            </svg>
                        </button>
                    )}
                </div>
                <button 
                    type="submit" 
                    className="search-button"
                    disabled={isLoading || !query.trim()}
                >
                    <span>Search</span>
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                        <line x1="5" y1="12" x2="19" y2="12"></line>
                        <polyline points="12 5 19 12 12 19"></polyline>
                    </svg>
                </button>
            </form>
        </div>
    )
}
