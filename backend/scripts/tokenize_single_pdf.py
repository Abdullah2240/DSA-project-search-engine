#!/usr/bin/env python3
"""
Tokenize a single PDF file for dynamic uploads.
Called by C++ PDFProcessor.

Usage:
    python tokenize_single_pdf.py <pdf_path> <doc_id> <output_json>
"""

import sys
import json
import re
import os
from typing import Optional

# Try to import PDF libraries
try:
    import PyPDF2
    PDF_LIB = "PyPDF2"
except ImportError:
    try:
        import pdfplumber
        PDF_LIB = "pdfplumber"
    except ImportError:
        print("ERROR: No PDF library found. Install PyPDF2 or pdfplumber", file=sys.stderr)
        sys.exit(1)


def tokenize(text: Optional[str]):
    """Tokenize text: lowercase, remove non-alphanumeric, split by whitespace."""
    if not text:
        return []
    text = re.sub(r"[^a-z0-9\s]", " ", text.lower())
    return [t for t in text.split() if len(t) > 1]


def extract_text_from_pdf(pdf_path: str, max_pages: int = 100) -> Optional[str]:
    """Extract text from PDF file."""
    try:
        if PDF_LIB == "PyPDF2":
            with open(pdf_path, "rb") as f:
                reader = PyPDF2.PdfReader(f)
                if not reader.pages:
                    return None
                text_parts = []
                for page in reader.pages[:max_pages]:
                    try:
                        text_parts.append(page.extract_text() or "")
                    except Exception:
                        continue
                return "\n".join(text_parts)
        
        elif PDF_LIB == "pdfplumber":
            import pdfplumber
            with pdfplumber.open(pdf_path) as pdf:
                text_parts = []
                for page in pdf.pages[:max_pages]:
                    try:
                        text_parts.append(page.extract_text() or "")
                    except Exception:
                        continue
                return "\n".join(text_parts)
    
    except Exception as e:
        print(f"ERROR extracting text: {e}", file=sys.stderr)
        return None


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
    
    # Extract text
    text = extract_text_from_pdf(pdf_path)
    if not text:
        print("ERROR: Could not extract text from PDF", file=sys.stderr)
        sys.exit(1)
    
    # Tokenize
    tokens = tokenize(text)
    if not tokens:
        print("ERROR: No tokens extracted", file=sys.stderr)
        sys.exit(1)
    
    # Extract title (first line or filename)
    lines = [line.strip() for line in text.split('\n') if line.strip()]
    title = lines[0][:200] if lines else os.path.basename(pdf_path).replace('.pdf', '')
    
    # Save result
    result = {
        "doc_id": doc_id,
        "title": title,
        "body_tokens": tokens,
        "word_count": len(tokens),
        "char_count": len(text),
    }
    
    os.makedirs(os.path.dirname(output_json), exist_ok=True)
    with open(output_json, "w", encoding="utf-8") as f:
        json.dump(result, f)
    
    print(f"SUCCESS: Tokenized {len(tokens)} tokens from {pdf_path}")


if __name__ == "__main__":
    main()
