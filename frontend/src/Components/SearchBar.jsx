import {useState} from "react"

export default function SearchBar({onSearch}) {
    const [query, setQuery] = useState("")

    const handleSubmit = e => {
        e.preventDefault()
        if((query.trimStart()).trimEnd() != "") {
            onSearch(query)
        }
    }

    return(
        <>
            <form onSubmit={handleSubmit}>
                <input type="text" className="search"  value={query} onChange={e => setQuery(e.target.value)} placeholder='Type your search'></input>
            </form>
        </>
    );
}
