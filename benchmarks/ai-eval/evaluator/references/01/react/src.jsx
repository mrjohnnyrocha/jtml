import React, {useState} from 'react';
import {createRoot} from 'react-dom/client';
function Reservation() {
  const [name,setName] = useState('');
  const [saved,setSaved] = useState('Not reserved');

  function reserve(e) { e.preventDefault(); setSaved(name === '' ? 'Name required' : `Reserved for ${name}`); }
  function reset() { setName(''); setSaved('Not reserved'); }
  return <main><h1>Reservation</h1><form onSubmit={reserve}>
    <label>Guest name<input value={name} onChange={e=>setName(e.target.value)}/></label>

    <button type="submit">Reserve</button><button type="button" onClick={reset}>Reset</button>
  </form><p>{saved}</p></main>;
}
createRoot(document.getElementById('root')).render(<Reservation/>);
