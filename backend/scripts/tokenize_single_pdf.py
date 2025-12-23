#!/usr/bin/env python3
"""
Optimized streaming PDF tokenizer for dynamic uploads.
Called by C++ PDFProcessor.

Usage:
    python tokenize_single_pdf.py <pdf_path> <doc_id> <output_json>
"""

import sys
import json
import re
import os
from typing import Optional, List, Tuple

# Try to import PDF libraries
try:
    import fitz  # PyMuPDF
    PDF_LIB = "PyMuPDF"
except ImportError:
    try:
        import pdfplumber
        PDF_LIB = "pdfplumber"
    except ImportError:
        print("ERROR: No PDF library found. Install PyMuPDF or pdfplumber", file=sys.stderr)
        sys.exit(1)


# Pre-compiled regex for tokenization (5-10x faster)
TOKEN_PATTERN = re.compile(r'[a-z0-9]+')

# OPTIMIZATION: Limits for fast dynamic uploads
MAX_PAGES_FOR_UPLOAD = 20  # Reduced from 100
MAX_TOKENS_FOR_UPLOAD = 5000  # Hard limit on tokens


def tokenize_fast(text: str) -> List[str]:
    """Fast tokenization using pre-compiled regex."""
    if not text:
        return []
    return TOKEN_PATTERN.findall(text.lower())


def extract_title_from_pdf(pdf_path: str) -> str:
    """Extract title from PDF with multiple fallback strategies."""
    try:
        if PDF_LIB == "PyMuPDF":
            doc = fitz.open(pdf_path)
            if doc.page_count == 0:
                doc.close()
                return os.path.basename(pdf_path).replace('.pdf', '')
            
            # Strategy 1: Try PDF metadata
            metadata = doc.metadata
            if metadata and metadata.get('title'):
                title = metadata['title'].strip()
                if title and len(title) > 3 and len(title) < 500:
                    doc.close()
                    return title[:200]
            
            # Strategy 2: Extract from first page
            first_page = doc.load_page(0)
            first_text = first_page.get_text()
            
            # Look for title-like text (larger font, first lines)
            lines = [line.strip() for line in first_text.split('\n') if line.strip()]
            
            # Filter out very short lines (likely headers/page numbers)
            candidate_lines = [line for line in lines if len(line) > 10 and len(line) < 300]
            
            if candidate_lines:
                # Take first substantial line as title
                title = candidate_lines[0][:200]
                doc.close()
                return title
            elif lines:
                # Fallback to any first line
                title = lines[0][:200]
                doc.close()
                return title
            
            doc.close()
            
        elif PDF_LIB == "pdfplumber":
            with pdfplumber.open(pdf_path) as pdf:
                if len(pdf.pages) == 0:
                    return os.path.basename(pdf_path).replace('.pdf', '')
                
                # Try metadata first
                if pdf.metadata and pdf.metadata.get('Title'):
                    title = pdf.metadata['Title'].strip()
                    if title and len(title) > 3 and len(title) < 500:
                        return title[:200]
                
                # Extract from first page
                first_page_text = pdf.pages[0].extract_text() or ""
                lines = [line.strip() for line in first_page_text.split('\n') if line.strip()]
                
                candidate_lines = [line for line in lines if len(line) > 10 and len(line) < 300]
                
                if candidate_lines:
                    return candidate_lines[0][:200]
                elif lines:
                    return lines[0][:200]
    
    except Exception as e:
        print(f"Warning: Error extracting title: {e}", file=sys.stderr)
    
    # Final fallback: use filename
    return os.path.basename(pdf_path).replace('.pdf', '').replace('_', ' ').replace('-', ' ')


def extract_text_streaming(pdf_path: str, max_pages: int = MAX_PAGES_FOR_UPLOAD, max_tokens: int = MAX_TOKENS_FOR_UPLOAD) -> Tuple[Optional[str], List[str]]:
    """
    Extract text with streaming tokenization.
    Returns: (title, tokens)
    OPTIMIZED: Stops early when max_tokens reached
    """
    try:
        # Extract title first
        title = extract_title_from_pdf(pdf_path)
        
        if PDF_LIB == "PyMuPDF":
            doc = fitz.open(pdf_path)
            if doc.page_count == 0:
                doc.close()
                return None, []
            
            # Stream tokenization page-by-page (stops at max_tokens)
            all_tokens = []
            pages_to_process = min(doc.page_count, max_pages)
            
            for page_num in range(pages_to_process):
                try:
                    page = doc.load_page(page_num)
                    page_text = page.get_text()
                    page_tokens = tokenize_fast(page_text)
                    all_tokens.extend(page_tokens)
                    
                    # OPTIMIZATION: Stop early if we have enough tokens
                    if len(all_tokens) >= max_tokens:
                        all_tokens = all_tokens[:max_tokens]
                        print(f"[Tokenizer] Reached {max_tokens} token limit at page {page_num + 1}/{doc.page_count}", file=sys.stderr)
                        break
                        
                except Exception as e:
                    print(f"Warning: Error on page {page_num}: {e}", file=sys.stderr)
                    continue
            
            doc.close()
            
            # Filter tokens: length > 1
            all_tokens = [t for t in all_tokens if len(t) > 1]
            
            return title, all_tokens
        
        elif PDF_LIB == "pdfplumber":
            with pdfplumber.open(pdf_path) as pdf:
                if len(pdf.pages) == 0:
                    return None, []
                
                # Stream tokenization with early stopping
                all_tokens = []
                pages_to_process = min(len(pdf.pages), max_pages)
                
                for page_num in range(pages_to_process):
                    try:
                        page_text = pdf.pages[page_num].extract_text() or ""
                        page_tokens = tokenize_fast(page_text)
                        all_tokens.extend(page_tokens)
                        
                        # OPTIMIZATION: Stop early
                        if len(all_tokens) >= max_tokens:
                            all_tokens = all_tokens[:max_tokens]
                            print(f"[Tokenizer] Reached {max_tokens} token limit at page {page_num + 1}/{len(pdf.pages)}", file=sys.stderr)
                            break
                            
                    except Exception as e:
                        print(f"Warning: Error on page {page_num}: {e}", file=sys.stderr)
                        continue
                
                # Filter tokens: length > 1
                all_tokens = [t for t in all_tokens if len(t) > 1]
                
                return title, all_tokens
    
    except Exception as e:
        print(f"ERROR extracting text: {e}", file=sys.stderr)
        return None, []


def main():
    if len(sys.argv) != 4:
        print("Usage: tokenize_single_pdf.py <pdf_path> <doc_id> <output_json>", file=sys.stderr)
        sys.exit(1)
    
    pdf_path = sys.argv[1]
    doc_id = int(sys.argv[2])
    output_json = sys.argv[3]
    
    if not os.path.exists(pdf_path):
        print(f"ERROR: PDF not found: {pdf_path}", file=sys.stderr)
        sys.exit(1)
    
    # Extract and tokenize with streaming (OPTIMIZED)
    title, tokens = extract_text_streaming(pdf_path)
    
    if not title or not tokens:
        print("ERROR: Could not extract text from PDF", file=sys.stderr)
        sys.exit(1)
    
    print(f"[Tokenizer] Extracted title: {title[:50]}...", file=sys.stderr)
    print(f"[Tokenizer] Extracted {len(tokens)} tokens", file=sys.stderr)
    
    # Save result
    result = {
        "doc_id": doc_id,
        "title": title,
        "body_tokens": tokens,
        "word_count": len(tokens),
    }
    
    os.makedirs(os.path.dirname(output_json), exist_ok=True)
    with open(output_json, "w", encoding="utf-8") as f:
        json.dump(result, f)
    
    print(f"SUCCESS: Tokenized {len(tokens)} tokens from {pdf_path}")


if __name__ == "__main__":
    main()
