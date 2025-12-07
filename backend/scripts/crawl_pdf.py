import requests
import json
import time
import re
import os
import tempfile
from urllib.parse import urlparse
import sys

# Try to import PDF libraries
try:
    import PyPDF2
    PDF_LIB = "PyPDF2"
except ImportError:
    try:
        import pdfplumber
        PDF_LIB = "pdfplumber"
    except ImportError:
        print("ERROR: No PDF library found. Install one of:")
        print("  pip install PyPDF2")
        print("  pip install pdfplumber")
        sys.exit(1)

# Get the directory of this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_FILE = os.path.join(SCRIPT_DIR, "..", "data", "processed", "cleaned.jsonl")
OUTPUT_FILE = os.path.join(SCRIPT_DIR, "..", "data", "processed", "cleaned_with_body.jsonl")
TEMP_DIR = os.path.join(SCRIPT_DIR, "..", "data", "temp_pdfs")

# Ensure temp directory exists
os.makedirs(TEMP_DIR, exist_ok=True)

def tokenize(text):
    """
    Lowercase, remove non-alphanumeric, split by whitespace
    """
    text = text.lower()
    text = re.sub(r"[^a-z0-9\s]", " ", text)
    tokens = [t for t in text.split() if t and len(t) > 1]  # Filter single chars
    return tokens

def extract_citations(text):
    """
    Extracts citations from PDF text
    Returns a list of citation strings found in the text
    """
    citations = []
    
    # Common citation patterns:
    # 1. Numbered citations: [1], [2-5], [1,2,3]
    numbered_pattern = r'\[(\d+(?:[,\s-]\d+)*)\]'
    numbered_matches = re.findall(numbered_pattern, text)
    for match in numbered_matches:
        # Split by comma or dash to get individual citation numbers
        numbers = re.split(r'[,\s-]+', match)
        citations.extend([f"[{num}]" for num in numbers if num.strip()])
    
    # 2. Author-year citations: (Smith, 2020), (Smith et al., 2020)
    author_year_pattern = r'\(([A-Z][a-z]+(?:\s+et\s+al\.)?,?\s*\d{4})\)'
    author_year_matches = re.findall(author_year_pattern, text)
    citations.extend([f"({match})" for match in author_year_matches])
    
    # 3. References section - look for common patterns
    # Find "References" or "Bibliography" section
    ref_section_pattern = r'(?:References|Bibliography|Works\s+Cited)[:\s]*(.*?)(?=\n\n[A-Z]|\Z)'
    ref_section_match = re.search(ref_section_pattern, text, re.IGNORECASE | re.DOTALL)
    if ref_section_match:
        ref_text = ref_section_match.group(1)
        # Extract lines that look like citations (contain author names and years)
        ref_lines = ref_text.split('\n')
        for line in ref_lines:
            line = line.strip()
            # Look for lines with author names (capitalized words) and years (4 digits)
            if re.search(r'[A-Z][a-z]+\s+[A-Z]', line) and re.search(r'\b(19|20)\d{2}\b', line):
                if len(line) > 20:  # Reasonable citation length
                    citations.append(line[:200])  # Limit length
    
    # Remove duplicates while preserving order
    seen = set()
    unique_citations = []
    for cit in citations:
        cit_lower = cit.lower().strip()
        if cit_lower and cit_lower not in seen:
            seen.add(cit_lower)
            unique_citations.append(cit)
    
    return unique_citations

def is_valid_pdf_url(url):
    """
    Validates PDF URL before attempting download
    """
    if not url or not isinstance(url, str):
        return False
    
    # Check URL format
    parsed = urlparse(url)
    if not parsed.scheme or not parsed.netloc:
        return False
    
    # Check if it's a PDF (by extension or content-type hint)
    url_lower = url.lower()
    if not (url_lower.endswith('.pdf') or 'pdf' in url_lower):
        # Still allow if it looks like a valid URL
        pass
    
    return True

def download_pdf(url, timeout=30, max_retries=3):
    """
    Downloads PDF from URL with validation
    Returns file path if successful, None otherwise
    """
    if not is_valid_pdf_url(url):
        return None
    
    for attempt in range(max_retries):
        try:
            headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
            }
            response = requests.get(url, headers=headers, timeout=timeout, stream=True)
            
            # Check status code
            if response.status_code != 200:
                if attempt < max_retries - 1:
                    time.sleep(2 ** attempt)  # Exponential backoff
                    continue
                return None
            
            # Check content type
            content_type = response.headers.get('content-type', '').lower()
            if 'pdf' not in content_type and not url.lower().endswith('.pdf'):
                # Still try to process if it might be a PDF
                pass
            
            # Check file size (max 50MB)
            content_length = response.headers.get('content-length')
            if content_length and int(content_length) > 50 * 1024 * 1024:
                print(f"  Skipping: PDF too large ({int(content_length) / 1024 / 1024:.1f}MB)")
                return None
            
            # Download to temp file
            temp_file = tempfile.NamedTemporaryFile(
                dir=TEMP_DIR,
                suffix='.pdf',
                delete=False
            )
            
            # Stream download
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    temp_file.write(chunk)
            
            temp_file.close()
            
            # Verify it's actually a PDF by checking first bytes
            with open(temp_file.name, 'rb') as f:
                first_bytes = f.read(4)
                if first_bytes != b'%PDF':
                    os.unlink(temp_file.name)
                    return None
            
            return temp_file.name
            
        except requests.exceptions.Timeout:
            if attempt < max_retries - 1:
                time.sleep(2 ** attempt)
                continue
            return None
        except requests.exceptions.RequestException as e:
            if attempt < max_retries - 1:
                time.sleep(2 ** attempt)
                continue
            return None
        except Exception as e:
            print(f"  Error downloading PDF: {e}")
            return None
    
    return None

def extract_text_from_pdf(pdf_path):
    """
    Extracts text from PDF file
    Returns extracted text or None if extraction fails
    """
    try:
        text = ""
        
        if PDF_LIB == "PyPDF2":
            with open(pdf_path, 'rb') as f:
                pdf_reader = PyPDF2.PdfReader(f)
                if len(pdf_reader.pages) == 0:
                    return None
                
                # Extract text from all pages (limit to first 100 pages for performance)
                max_pages = min(100, len(pdf_reader.pages))
                for i in range(max_pages):
                    try:
                        page = pdf_reader.pages[i]
                        page_text = page.extract_text()
                        if page_text:
                            text += page_text + " "
                    except Exception as e:
                        continue
        
        elif PDF_LIB == "pdfplumber":
            with pdfplumber.open(pdf_path) as pdf:
                if len(pdf.pages) == 0:
                    return None
                
                # Extract text from all pages (limit to first 100 pages)
                max_pages = min(100, len(pdf.pages))
                for i in range(max_pages):
                    try:
                        page = pdf.pages[i]
                        page_text = page.extract_text()
                        if page_text:
                            text += page_text + " "
                    except Exception as e:
                        continue
        
        return text.strip() if text else None
        
    except Exception as e:
        print(f"  Error extracting text: {e}")
        return None

def process_pdf_entry(entry):
    """
    Processes a single entry: downloads PDF, extracts text, tokenizes
    Preserves page_rank data for ranking purposes
    Returns updated entry or None if processing fails
    """
    pdf_url = entry.get("url")
    if not pdf_url:
        return None
    
    # Get page rank info for display
    page_rank = entry.get("page_rank", {})
    cited_count = page_rank.get("cited_by_count", 0) if page_rank else 0
    
    print(f"Processing doc_id {entry.get('doc_id')} (citations: {cited_count}): {pdf_url[:80]}...")
    
    # Download PDF
    pdf_path = download_pdf(pdf_url)
    if not pdf_path:
        print(f"  Failed to download PDF")
        return None
    
    try:
        # Extract text
        text = extract_text_from_pdf(pdf_path)
        if not text or len(text) < 100:  # Skip if too short (might be image-only PDF)
            print(f"  No text extracted or text too short")
            return None
        
        # Extract citations from the PDF text
        citations = extract_citations(text)
        
        # Tokenize
        body_tokens = tokenize(text)
        if len(body_tokens) < 50:  # Skip if too few tokens
            print(f"  Too few tokens extracted ({len(body_tokens)})")
            return None
        
        # Update entry - preserve page_rank data for ranking
        entry["body_tokens"] = body_tokens
        
        # Store extracted citations
        if citations:
            entry["citations"] = citations
        
        # Ensure page_rank is preserved (should already be there from getData.py)
        if "page_rank" not in entry:
            entry["page_rank"] = {}
        
        # Get citation count for display (re-fetch in case entry was modified)
        page_rank_final = entry.get("page_rank", {})
        cited_count_final = page_rank_final.get("cited_by_count", 0) if isinstance(page_rank_final, dict) else 0
        citations_count = len(citations) if citations else 0
        print(f"  Success: Extracted {len(body_tokens)} tokens, {citations_count} citations found (rank: {cited_count_final} citations)")
        
        return entry
        
    finally:
        # Clean up temp file
        try:
            if os.path.exists(pdf_path):
                os.unlink(pdf_path)
        except:
            pass

def main():
    print(f"PDF Crawler using {PDF_LIB}")
    print(f"Input file: {INPUT_FILE}")
    print(f"Output file: {OUTPUT_FILE}")
    print()
    
    if not os.path.exists(INPUT_FILE):
        print(f"ERROR: Input file not found: {INPUT_FILE}")
        print("Run getData.py first to generate the input file.")
        sys.exit(1)
    
    processed = 0
    failed = 0
    skipped = 0
    
    # Read all entries first
    entries = []
    print("Reading entries from input file...")
    with open(INPUT_FILE, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            if not line.strip():
                continue
            try:
                entry = json.loads(line)
                entries.append(entry)
            except json.JSONDecodeError as e:
                print(f"Error parsing line {line_num}: {e}")
                skipped += 1
                continue
    
    print(f"Found {len(entries)} entries to process")
    
    # Sort by page rank (cited_by_count) - process higher-ranked papers first
    # This ensures we get the most important content first
    def get_citation_count(entry):
        page_rank = entry.get("page_rank", {})
        if isinstance(page_rank, dict):
            return page_rank.get("cited_by_count", 0)
        return 0
    
    entries_sorted = sorted(entries, key=get_citation_count, reverse=True)
    entries = entries_sorted
    
    # Show ranking statistics
    total_citations = sum(get_citation_count(e) for e in entries)
    avg_citations = total_citations / len(entries) if entries else 0
    max_citations = max((get_citation_count(e) for e in entries), default=0)
    
    print(f"  Total citations: {total_citations:,}")
    print(f"  Average citations per paper: {avg_citations:.1f}")
    print(f"  Highest cited paper: {max_citations:,} citations")
    print(f"  Processing order: Highest to lowest citations (for better ranking data)\n")
    
    # Process entries
    with open(OUTPUT_FILE, "w", encoding="utf-8") as out_f:
        for i, entry in enumerate(entries, 1):
            # Check if already has body_tokens (resume capability)
            if entry.get("body_tokens"):
                page_rank = entry.get("page_rank", {})
                cited_count = page_rank.get("cited_by_count", 0) if isinstance(page_rank, dict) else 0
                print(f"Entry {i}/{len(entries)} (rank: {cited_count}): Already has body_tokens, skipping")
                # Ensure page_rank is preserved
                if "page_rank" not in entry:
                    entry["page_rank"] = {}
                out_f.write(json.dumps(entry, ensure_ascii=False) + "\n")
                processed += 1
                continue
            
            # Process PDF
            updated_entry = process_pdf_entry(entry)
            if updated_entry:
                # Ensure page_rank is preserved in output
                if "page_rank" not in updated_entry:
                    updated_entry["page_rank"] = entry.get("page_rank", {})
                out_f.write(json.dumps(updated_entry, ensure_ascii=False) + "\n")
                processed += 1
            else:
                failed += 1
                # Still write the entry without body_tokens (for tracking)
                # But preserve page_rank data
                if "page_rank" not in entry:
                    entry["page_rank"] = {}
                out_f.write(json.dumps(entry, ensure_ascii=False) + "\n")
            
            # Progress update
            if i % 10 == 0:
                print(f"\nProgress: {i}/{len(entries)} processed ({processed} successful, {failed} failed)\n")
            
            # Rate limiting
            time.sleep(0.5)  # Be polite to servers
    
    print(f"\n{'='*60}")
    print(f"Processing complete!")
    print(f"  Total entries: {len(entries)}")
    print(f"  Successfully processed: {processed}")
    print(f"  Failed: {failed}")
    print(f"  Skipped: {skipped}")
    print(f"\nOutput saved to: {OUTPUT_FILE}")
    print(f"{'='*60}")

if __name__ == "__main__":
    main()
