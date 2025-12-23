import { useState, useRef, useEffect } from 'react'
import './PDFUpload.css'
import { API_ENDPOINTS } from '../config/api'

function PDFUpload({ onSuccess }) {
  const [isDragging, setIsDragging] = useState(false)
  const [selectedFiles, setSelectedFiles] = useState([])
  const [isUploading, setIsUploading] = useState(false)
  const [uploadProgress, setUploadProgress] = useState(null)
  const [error, setError] = useState(null)
  const fileInputRef = useRef(null)
  const pollIntervalRef = useRef(null)

  // Poll upload progress
  useEffect(() => {
    if (!isUploading) {
      // Clear interval when not uploading
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current)
        pollIntervalRef.current = null
      }
      return
    }

    // Start polling
    pollIntervalRef.current = setInterval(async () => {
      try {
        const res = await fetch(API_ENDPOINTS.UPLOAD_PROGRESS)
        if (res.ok) {
          const progress = await res.json()
          setUploadProgress(progress)
          console.log('Progress:', progress) // Debug
        }
      } catch (err) {
        console.error('Progress poll failed:', err)
      }
    }, 300) // Poll every 300ms for faster updates

    return () => {
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current)
      }
    }
  }, [isUploading])

  const handleDragEnter = (e) => {
    e.preventDefault()
    e.stopPropagation()
    setIsDragging(true)
  }

  const handleDragLeave = (e) => {
    e.preventDefault()
    e.stopPropagation()
    setIsDragging(false)
  }

  const handleDragOver = (e) => {
    e.preventDefault()
    e.stopPropagation()
  }

  const handleDrop = (e) => {
    e.preventDefault()
    e.stopPropagation()
    setIsDragging(false)

    const files = Array.from(e.dataTransfer.files).filter(file => 
      file.type === 'application/pdf'
    )
    
    if (files.length > 0) {
      setSelectedFiles(prev => [...prev, ...files])
    }
  }

  const handleFileSelect = (e) => {
    const files = Array.from(e.target.files).filter(file => 
      file.type === 'application/pdf'
    )
    
    if (files.length > 0) {
      setSelectedFiles(prev => [...prev, ...files])
    }
  }

  const handleUpload = async () => {
    if (selectedFiles.length === 0) return
    
    setIsUploading(true)
    setUploadProgress({ total: selectedFiles.length, processed: 0, indexed: 0, status: ['Uploading files...'] })
    setError(null)
    
    const formData = new FormData()
    selectedFiles.forEach(file => {
      formData.append('files', file)
    })

    try {
      const response = await fetch(API_ENDPOINTS.UPLOAD, {
        method: 'POST',
        body: formData,
      })

      if (!response.ok) {
        throw new Error('Upload failed')
      }

      const result = await response.json()
      
      // Show final completion status
      setUploadProgress({
        total: selectedFiles.length,
        processed: result.uploadedCount || selectedFiles.length,
        indexed: result.uploadedCount || selectedFiles.length,
        status: ['✅ Upload complete! Documents are now searchable.']
      })
      
      // Clear after 3 seconds
      setTimeout(() => {
        setIsUploading(false)
        setUploadProgress(null)
        setSelectedFiles([])
        if (onSuccess) onSuccess(result)
      }, 3000)
      
    } catch (err) {
      console.error('Upload error:', err)
      setError('Failed to upload files. Please try again.')
      setIsUploading(false)
      setUploadProgress(null)
    }
  }

  const handleClick = () => {
    fileInputRef.current?.click()
  }

  const removeFile = (index) => {
    const newFiles = selectedFiles.filter((_, i) => i !== index)
    setSelectedFiles(newFiles)
  }

  // Calculate progress percentage
  const getProgressPercent = () => {
    if (!uploadProgress || uploadProgress.total === 0) return 0
    
    // 30% for upload, 50% for processing, 20% for indexing
    const uploadPhase = Math.min(30, 30) // Assume upload completes quickly
    const processingPhase = (uploadProgress.processed / uploadProgress.total) * 50
    const indexingPhase = (uploadProgress.indexed / uploadProgress.total) * 20
    
    return Math.min(100, uploadPhase + processingPhase + indexingPhase)
  }

  return (
    <div className="pdf-upload-container">
      {error && (
        <div className="upload-error">
          <span>⚠️ {error}</span>
          <button onClick={() => setError(null)}>×</button>
        </div>
      )}
      
      <div 
        className={`pdf-upload-dropzone ${isDragging ? 'dragging' : ''}`}
        onDragEnter={handleDragEnter}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        onClick={handleClick}
      >
        <input
          ref={fileInputRef}
          type="file"
          accept=".pdf"
          multiple
          onChange={handleFileSelect}
          className="pdf-upload-input"
        />
        <div className="upload-icon">
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
            <polyline points="17 8 12 3 7 8"></polyline>
            <line x1="12" y1="3" x2="12" y2="15"></line>
          </svg>
        </div>
        <p className="upload-text">
          {isDragging ? 'Drop PDFs here' : 'Drag & drop PDFs or click to browse'}
        </p>
        <p className="upload-hint">⚡ Fast indexing: ~30 seconds per document</p>
      </div>

      {selectedFiles.length > 0 && (
        <div className="uploaded-files">
          <div className="files-header">
            <span className="files-count">{selectedFiles.length} file{selectedFiles.length !== 1 ? 's' : ''} selected</span>
          </div>
          <div className="files-list">
            {selectedFiles.map((file, index) => (
              <div key={index} className="file-item">
                <span className="file-name" title={file.name}>{file.name}</span>
                <span className="file-size">{(file.size / 1024 / 1024).toFixed(2)} MB</span>
                <button 
                  className="remove-file-btn"
                  onClick={(e) => {
                    e.stopPropagation()
                    removeFile(index)
                  }}
                  aria-label="Remove file"
                >
                  ×
                </button>
              </div>
            ))}
          </div>
          <button 
            className="upload-button"
            onClick={handleUpload}
            disabled={isUploading}
          >
            {isUploading ? (
              <div className="upload-progress-container">
                <div className="progress-bar-wrapper">
                  <div 
                    className="progress-bar" 
                    style={{ width: `${getProgressPercent()}%` }}
                  ></div>
                </div>
                <div className="progress-info">
                  <span className="progress-percent">{getProgressPercent().toFixed(0)}%</span>
                  {uploadProgress && (
                    <div className="progress-details">
                      {uploadProgress.status && uploadProgress.status.length > 0 && (
                        <p className="progress-status">{uploadProgress.status[0]}</p>
                      )}
                      <p className="progress-counts">
                        {uploadProgress.processed > 0 && (
                          <span>Processed: {uploadProgress.processed}/{uploadProgress.total}</span>
                        )}
                        {uploadProgress.indexed > 0 && (
                          <span> | Indexed: {uploadProgress.indexed}/{uploadProgress.total}</span>
                        )}
                      </p>
                    </div>
                  )}
                </div>
              </div>
            ) : (
              <>
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                  <polyline points="7 10 12 15 17 10"></polyline>
                  <line x1="12" y1="15" x2="12" y2="3"></line>
                </svg>
                Upload & Index Documents
              </>
            )}
          </button>
        </div>
      )}
    </div>
  )
}

export default PDFUpload

