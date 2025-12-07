import { useState, useRef, useEffect } from "react"
import { useAutocomplete } from "../hooks/useAutocomplete"
import './SearchBar.css'

export default function SearchBar({ onSearch, isLoading }) {
    const [query, setQuery] = useState("")
    const [showSuggestions, setShowSuggestions] = useState(false)
    const [selectedIndex, setSelectedIndex] = useState(-1)
    const inputRef = useRef(null)
    const suggestionsRef = useRef(null)
    const { suggestions, isLoading: isLoadingSuggestions } = useAutocomplete(query, showSuggestions)

    const handleSubmit = e => {
        e.preventDefault()
        const trimmedQuery = query.trim()
        if (trimmedQuery !== "") {
            setShowSuggestions(false)
            onSearch(trimmedQuery)
        }
    }

    const handleKeyDown = (e) => {
        if (e.key === 'Enter') {
            if (selectedIndex >= 0 && suggestions.length > 0) {
                e.preventDefault()
                handleSuggestionClick(suggestions[selectedIndex])
            } else {
                handleSubmit(e)
            }
        } else if (e.key === 'ArrowDown') {
            e.preventDefault()
            setSelectedIndex(prev => 
                prev < suggestions.length - 1 ? prev + 1 : prev
            )
        } else if (e.key === 'ArrowUp') {
            e.preventDefault()
            setSelectedIndex(prev => prev > 0 ? prev - 1 : -1)
        } else if (e.key === 'Escape') {
            setShowSuggestions(false)
            setSelectedIndex(-1)
        }
    }

    const handleSuggestionClick = (suggestion) => {
        setQuery(suggestion)
        setShowSuggestions(false)
        setSelectedIndex(-1)
        onSearch(suggestion)
        inputRef.current?.blur()
    }

    const handleInputChange = (e) => {
        setQuery(e.target.value)
        setShowSuggestions(true)
        setSelectedIndex(-1)
    }

    const handleInputFocus = () => {
        if (suggestions.length > 0) {
            setShowSuggestions(true)
        }
    }

    const handleInputBlur = (e) => {
        // Don't hide if clicking on a suggestion
        if (!suggestionsRef.current?.contains(e.relatedTarget)) {
            setTimeout(() => setShowSuggestions(false), 200)
        }
    }

    // Scroll selected suggestion into view
    useEffect(() => {
        if (selectedIndex >= 0 && suggestionsRef.current) {
            const selectedElement = suggestionsRef.current.children[selectedIndex]
            if (selectedElement) {
                selectedElement.scrollIntoView({ block: 'nearest', behavior: 'smooth' })
            }
        }
    }, [selectedIndex])

    // Reset selected index when suggestions change
    useEffect(() => {
        setSelectedIndex(-1)
    }, [suggestions])

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
                        ref={inputRef}
                        type="text" 
                        className="search-input"  
                        value={query} 
                        onChange={handleInputChange}
                        onKeyDown={handleKeyDown}
                        onFocus={handleInputFocus}
                        onBlur={handleInputBlur}
                        placeholder="Search the cosmos..." 
                        disabled={isLoading}
                        autoFocus
                        autoComplete="off"
                    />
                    {isLoadingSuggestions && query.length >= 2 && (
                        <div className="search-loading">
                            <div className="mini-spinner"></div>
                        </div>
                    )}
                    {query && !isLoading && !isLoadingSuggestions && (
                        <button 
                            type="button"
                            className="clear-button"
                            onClick={() => {
                                setQuery("")
                                setShowSuggestions(false)
                                inputRef.current?.focus()
                            }}
                            aria-label="Clear search"
                        >
                            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                                <line x1="18" y1="6" x2="6" y2="18"></line>
                                <line x1="6" y1="6" x2="18" y2="18"></line>
                            </svg>
                        </button>
                    )}
                    
                    {/* Autocomplete Suggestions Dropdown */}
                    {showSuggestions && suggestions.length > 0 && query.length >= 2 && (
                        <div 
                            ref={suggestionsRef}
                            className="autocomplete-dropdown"
                        >
                            {suggestions.map((suggestion, index) => {
                                const queryLower = query.toLowerCase()
                                const suggestionLower = suggestion.toLowerCase()
                                const matchIndex = suggestionLower.indexOf(queryLower)
                                
                                return (
                                    <div
                                        key={index}
                                        className={`autocomplete-item ${index === selectedIndex ? 'selected' : ''}`}
                                        onClick={() => handleSuggestionClick(suggestion)}
                                        onMouseEnter={() => setSelectedIndex(index)}
                                    >
                                        {matchIndex >= 0 ? (
                                            <>
                                                <span>{suggestion.substring(0, matchIndex)}</span>
                                                <span className="highlight">
                                                    {suggestion.substring(matchIndex, matchIndex + query.length)}
                                                </span>
                                                <span>{suggestion.substring(matchIndex + query.length)}</span>
                                            </>
                                        ) : (
                                            <span>{suggestion}</span>
                                        )}
                                    </div>
                                )
                            })}
                        </div>
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
