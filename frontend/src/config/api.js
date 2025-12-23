// API Configuration
// Development: use direct backend URL
// Production: use relative paths (when served from backend)
const isDevelopment = import.meta.env.DEV;

export const API_BASE_URL = isDevelopment ? 'http://localhost:8080' : '';

// API Endpoints
export const API_ENDPOINTS = {
  SEARCH: `${API_BASE_URL}/search`,
  AUTOCOMPLETE: `${API_BASE_URL}/autocomplete`,
  UPLOAD: `${API_BASE_URL}/upload`,
  UPLOAD_PROGRESS: `${API_BASE_URL}/upload-progress`,
  DOWNLOAD: (docId) => `${API_BASE_URL}/download/${docId}`,
  STATS: `${API_BASE_URL}/stats`,
};

// Helper function to build search URL
export const buildSearchUrl = (query) => {
  return `${API_ENDPOINTS.SEARCH}?q=${encodeURIComponent(query)}`;
};

// Helper function to build autocomplete URL
export const buildAutocompleteUrl = (prefix, limit = 8) => {
  return `${API_ENDPOINTS.AUTOCOMPLETE}?q=${encodeURIComponent(prefix)}&limit=${limit}`;
};

// Helper function to build download URL
export const buildDownloadUrl = (docId) => {
  return `${API_BASE_URL}/download/${docId}`;
};