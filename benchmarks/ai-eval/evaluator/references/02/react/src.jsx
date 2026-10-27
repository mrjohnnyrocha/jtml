import React, {useState} from 'react';
import {createRoot} from 'react-dom/client';
function Reservation() {
  const [name,setName] = useState('');
  const [saved,setSaved] = useState('Not reserved');
  const [size,setSize] = useState(1);
  function reserve(e) { e.preventDefault(); setSaved(name === '' ? 'Name required' : `Reserved for ${name} (${size} guests)`); }
  function reset() { setName(''); setSaved('Not reserved'); setSize(1); }
  return <main><h1>Reservation</h1><form onSubmit={reserve}>
    <label>Guest name<input value={name} onChange={e=>setName(e.target.value)}/></label>
    <p>Guests: {size}</p><p>Total: EUR {size * 20}</p>
    <button type="button" onClick={()=>setSize(Math.min(6,size+1))}>Add guest</button>
    <button type="button" onClick={()=>setSize(Math.max(1,size-1))}>Remove guest</button>
    <button type="submit">Reserve</button><button type="button" onClick={reset}>Reset</button>
  </form><p>{saved}</p></main>;
}
createRoot(document.getElementById('root')).render(<Reservation/>);
