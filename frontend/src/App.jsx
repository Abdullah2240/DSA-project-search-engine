import SearchBar from './Components/SearchBar'
import {useState} from 'react'
import './App.css'

function App() {
  const [results, setResults] = useState([]);

  const performSearch = async(query) => {
    try{
      // const res = await fetch("")
      // data = await res.json()
      // setResults(data.results)
      setResults([4, 3, 24, "this is just ayevein data bruthas"])
    } catch (err){
      console.error("Error while searching", err)
    }
  }

  const autocomplete = async(query) => {
    try{
      const res = await("")
      data = await res.json
      setResults(data.results)
    } catch (err){
      console.error("Error while searching", err)
    }
  }

  return (
    <>
      <div className="container">
        <div className="textbar">
          <SearchBar onSearch={performSearch}/>
          <ul>
            {results.map((r, idx) => (
              <li key={idx}>{r}</li>
            ))}
          </ul>
        </div>
      </div>
      
    </>
  );
}

export default App
