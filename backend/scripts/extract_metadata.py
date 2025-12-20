#!/usr/bin/env python3
"""
extract_metadata.py
Extracts document metadata from cleaned.jsonl or cleaned_with_body.jsonl
Extracts: publication_year, publication_month, cited_by_count, title, url, keywords
"""

import json
import sys
import os
from datetime import datetime

def extract_year_from_date(date_str):
    """Extract year from various date formats"""
    if not date_str:
        return 0
    
    # Try parsing different date formats
    formats = [
        "%Y-%m-%d",
        "%Y-%m",
        "%Y",
        "%Y/%m/%d",
        "%d/%m/%Y",
        "%m/%d/%Y"
    ]
    
    for fmt in formats:
        try:
            dt = datetime.strptime(date_str[:10], fmt)
            return dt.year
        except:
            continue
    
    # Try extracting year as 4-digit number
    try:
        year = int(date_str[:4])
        if 1900 <= year <= 2100:
            return year
    except:
        pass
    
    return 0

def extract_month_from_date(date_str):
    """Extract month from date string"""
    if not date_str:
        return 0
    
    formats = [
        "%Y-%m-%d",
        "%Y-%m",
        "%Y/%m/%d",
        "%d/%m/%Y",
        "%m/%d/%Y"
    ]
    
    for fmt in formats:
        try:
            dt = datetime.strptime(date_str[:10], fmt)
            return dt.month
        except:
            continue
    
    return 0

def extract_metadata_from_entry(entry):
    """Extract metadata from a single JSONL entry"""
    metadata = {}
    
    # Extract doc_id (required)
    if "doc_id" not in entry:
        return None
    
    metadata["doc_id"] = entry["doc_id"]
    
    # Extract publication year
    pub_year = 0
    if "publication_date" in entry and entry["publication_date"]:
        pub_year = extract_year_from_date(entry["publication_date"])
    elif "year" in entry and entry["year"]:
        try:
            pub_year = int(entry["year"])
        except:
            pass
    
    metadata["publication_year"] = pub_year
    
    # Extract publication month
    pub_month = 0
    if "publication_date" in entry and entry["publication_date"]:
        pub_month = extract_month_from_date(entry["publication_date"])
    
    metadata["publication_month"] = pub_month
    
    # Extract cited_by_count from page_rank
    cited_by_count = 0
    if "page_rank" in entry and isinstance(entry["page_rank"], dict):
        cited_by_count = entry["page_rank"].get("cited_by_count", 0)
    
    metadata["cited_by_count"] = cited_by_count
    
    # Extract title
    title = entry.get("title", "")
    metadata["title"] = title
    
    # Extract URL
    url = entry.get("url", "")
    metadata["url"] = url
    
    # Extract keywords (if available)
    keywords = []
    if "keywords" in entry:
        if isinstance(entry["keywords"], list):
            keywords = entry["keywords"]
        elif isinstance(entry["keywords"], str):
            keywords = [k.strip() for k in entry["keywords"].split(",")]
    
    metadata["keywords"] = keywords
    
    return metadata

def main():
    # Get the directory of this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Try cleaned_with_body.jsonl first (from PDF crawler), fall back to cleaned.jsonl
    input_file_with_body = os.path.join(script_dir, "..", "data", "processed", "test.jsonl")
    input_file_basic = os.path.join(script_dir, "..", "data", "processed", "test.jsonl")
    output_file = os.path.join(script_dir, "..", "data", "processed", "document_metadata.json")
    
    # Allow command-line arguments to override
    if len(sys.argv) >= 2:
        input_file_with_body = sys.argv[1]
        input_file_basic = sys.argv[1]
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    
    # Determine which input file to use
    input_file = None
    if os.path.exists(input_file_with_body):
        input_file = input_file_with_body
        print(f"Using input file: {input_file} (with PDF body content)")
    elif os.path.exists(input_file_basic):
        input_file = input_file_basic
        print(f"Using input file: {input_file} (basic metadata only)")
    else:
        print(f"Error: No input file found!")
        print(f"Searched for:")
        print(f"  1. {input_file_with_body}")
        print(f"  2. {input_file_basic}")
        print()
        print("Please run one of the following first:")
        print("  1. python scripts/getData.py")
        print("  2. python scripts/crawl_pdf.py (after running getData.py)")
        sys.exit(1)
    
    print(f"Output will be saved to: {output_file}")
    
    metadata_dict = {}
    processed = 0
    errors = 0
    
    with open(input_file, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            if not line.strip():
                continue
            
            try:
                entry = json.loads(line)
                metadata = extract_metadata_from_entry(entry)
                
                if metadata:
                    doc_id = metadata["doc_id"]
                    # Store as {doc_id: {year, month, cited_count, ...}}
                    metadata_dict[str(doc_id)] = {
                        "publication_year": metadata.get("publication_year", 0),
                        "publication_month": metadata.get("publication_month", 0),
                        "cited_by_count": metadata.get("cited_by_count", 0),
                        "title": metadata.get("title", ""),
                        "url": metadata.get("url", ""),
                        "keywords": metadata.get("keywords", [])
                    }
                    processed += 1
                else:
                    errors += 1
                    
            except json.JSONDecodeError as e:
                print(f"Warning: JSON decode error on line {line_num}: {e}")
                errors += 1
            except Exception as e:
                print(f"Warning: Error processing line {line_num}: {e}")
                errors += 1
            
            if processed % 1000 == 0:
                print(f"Processed {processed} documents...")
    
    # Save to JSON file
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(metadata_dict, f, indent=2, ensure_ascii=False)
    
    print(f"\nDone!")
    print(f"Successfully processed: {processed} documents")
    print(f"Errors: {errors}")
    print(f"Metadata saved to: {output_file}")

if __name__ == "__main__":
    main()