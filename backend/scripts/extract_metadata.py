#!/usr/bin/env python3
"""
extract_metadata.py
Extracts document metadata from cleaned.jsonl and saves to document_metadata.json
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
    
    # Doc ID
    doc_id = entry.get("doc_id", -1)
    if doc_id == -1:
        return None
    
    metadata["doc_id"] = doc_id
    
    # Title
    if "title" in entry:
        metadata["title"] = entry["title"]
    
    # URL
    if "url" in entry:
        metadata["url"] = entry["url"]
    
    # Publication year and month
    publication_year = 0
    publication_month = 0
    
    # Try different fields for publication date
    date_fields = ["publication_date", "publication_year", "year", "date", "published_date", "release_date"]
    for field in date_fields:
        if field in entry and entry[field]:
            if field == "publication_year" or field == "year":
                try:
                    publication_year = int(entry[field])
                except:
                    pass
            else:
                date_str = str(entry[field])
                if not publication_year:
                    publication_year = extract_year_from_date(date_str)
                if not publication_month:
                    publication_month = extract_month_from_date(date_str)
            if publication_year:
                break
    
    metadata["publication_year"] = publication_year
    metadata["publication_month"] = publication_month
    
    # Citation count from page_rank
    cited_by_count = 0
    if "page_rank" in entry and isinstance(entry["page_rank"], dict):
        cited_by_count = entry["page_rank"].get("cited_by_count", 0)
    elif "cited_by_count" in entry:
        cited_by_count = entry.get("cited_by_count", 0)
    
    metadata["cited_by_count"] = cited_by_count
    
    # Keywords (if available)
    keywords = []
    if "keywords" in entry:
        if isinstance(entry["keywords"], list):
            keywords = entry["keywords"]
        elif isinstance(entry["keywords"], str):
            # Split comma-separated keywords
            keywords = [k.strip() for k in entry["keywords"].split(",")]
    
    metadata["keywords"] = keywords
    
    return metadata

def main():
    # Default paths
    input_file = "data/processed/cleaned.jsonl"
    output_file = "data/processed/document_metadata.json"
    
    # Allow command-line arguments
    if len(sys.argv) >= 2:
        input_file = sys.argv[1]
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    
    # Check if input file exists
    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)
    
    print(f"Extracting metadata from: {input_file}")
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

