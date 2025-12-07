import { useState, useEffect, useRef, useCallback } from 'react';

// Cache for autocomplete results
const autocompleteCache = new Map();
const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes

// Debounce utility
function useDebounce(value, delay) {
  const [debouncedValue, setDebouncedValue] = useState(value);

  useEffect(() => {
    const handler = setTimeout(() => {
      setDebouncedValue(value);
    }, delay);

    return () => {
      clearTimeout(handler);
    };
  }, [value, delay]);

  return debouncedValue;
}

export function useAutocomplete(query, enabled = true) {
  const [suggestions, setSuggestions] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);
  const abortControllerRef = useRef(null);

  // Debounce the query to avoid too many API calls
  const debouncedQuery = useDebounce(query, 300);

  const fetchSuggestions = useCallback(async (prefix) => {
    // Don't fetch if query is too short or disabled
    if (!enabled || prefix.trim().length < 2) {
      setSuggestions([]);
      return;
    }

    // Check cache first
    const cacheKey = prefix.toLowerCase().trim();
    const cached = autocompleteCache.get(cacheKey);
    if (cached && Date.now() - cached.timestamp < CACHE_DURATION) {
      setSuggestions(cached.data);
      return;
    }

    // Cancel previous request if any
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
    }

    abortControllerRef.current = new AbortController();
    setIsLoading(true);
    setError(null);

    try {
      const apiUrl = `http://localhost:8080/autocomplete?q=${encodeURIComponent(prefix)}&limit=8`;
      const res = await fetch(apiUrl, {
        signal: abortControllerRef.current.signal
      });

      if (!res.ok) {
        throw new Error(`Autocomplete failed: ${res.status}`);
      }

      const data = await res.json();
      const suggestionsList = data.suggestions || [];

      // Cache the results
      autocompleteCache.set(cacheKey, {
        data: suggestionsList,
        timestamp: Date.now()
      });

      setSuggestions(suggestionsList);
    } catch (err) {
      if (err.name !== 'AbortError') {
        console.error('Autocomplete error:', err);
        setError(err.message);
        setSuggestions([]);
      }
    } finally {
      setIsLoading(false);
    }
  }, [enabled]);

  useEffect(() => {
    fetchSuggestions(debouncedQuery);

    return () => {
      if (abortControllerRef.current) {
        abortControllerRef.current.abort();
      }
    };
  }, [debouncedQuery, fetchSuggestions]);

  // Clear suggestions when query is cleared
  useEffect(() => {
    if (!query || query.trim().length < 2) {
      setSuggestions([]);
    }
  }, [query]);

  return { suggestions, isLoading, error };
}

