/* Contributed ink stays decorative; all operating labels and figures remain live. */
var ManyspaceNotebookArt = (() => {
  const choices = [
    ['K1','kitty-1','cover','.page.left','footer',160,344/320,-5,'original'],
    ['C3','cat-1','play','[data-step="7"]','footer',120,306/240,5,'blue'],
    ['D1','doodle','generate','h1','beside',95,205/190,-8,'blue'],
    ['W1','doodle-warm','shape','h1','beside',105,190/220,6,'original'],
    ['W5','doodle-warm','appendix','.sec','section',110,190/220,-5,'blue'],
    ['I5','doodle-winter','appendix','.sec','credits',115,252/230,4,'graphite']
  ];
  function apply(pages) {
    choices.forEach(([id,asset,page,selector,placement,width,ratio,turn,color]) => {
      const target=placement==='section'?pages[page].querySelectorAll(selector)[2]:placement==='credits'?pages[page].querySelectorAll(selector)[4]:pages[page].querySelector(selector);
      const ink=document.createElement('span');ink.className='margin-doodle '+color;ink.dataset.placement=id;ink.setAttribute('aria-hidden','true');
      ink.style.cssText=`--ink:var(--art-${asset});--ink-width:${width}px;--ink-height:${Math.round(width*ratio)}px;--ink-turn:${turn}deg`;
      if(placement==='beside') {
        const row=document.createElement('div');row.className='margin-heading';target.replaceWith(row);row.append(target,ink);
      } else if(placement==='section') {target.classList.add('room-character-art');target.append(ink);}
      else {const footer=document.createElement('div');footer.className='margin-footer'+(['K1','I5'].includes(id)?' left':'');footer.append(ink);target.append(footer);}
    });
    const cover=pages.cover.querySelectorAll('.zone');
    cover[0].classList.add('contributed-photo');cover[0].removeAttribute('aria-hidden');cover[0].setAttribute('role','img');cover[0].setAttribute('aria-label','Contributed instant photograph of a cloth doll');
    cover[0].parentElement.querySelector('.cap').textContent='a little company';
    cover[1].classList.add('circuit-thumb');cover[1].parentElement.querySelector('.cap').textContent='64-room circuit';
    const mappingDescription='A hand-drawn card: six measured bits, paired by colour, each pair setting a knob on one of three ranges, spacing, speed and damping; the count of ones picks forward or reversed tails.';
    cover[2].classList.add('contributed-mapping');cover[2].removeAttribute('aria-hidden');cover[2].setAttribute('role','img');cover[2].setAttribute('aria-label',mappingDescription);
    const mappingFrame=cover[2].parentElement;mappingFrame.classList.add('mapping-frame');mappingFrame.querySelector('.cap').textContent='bits → room character';
    const mappingStudy=document.createElement('details');mappingStudy.className='mapping-study';
    const mappingSummary=document.createElement('summary');mappingSummary.textContent='Inspect the mapping card';
    const mappingImage=document.createElement('div');mappingImage.className='contributed-mapping';mappingImage.setAttribute('role','img');mappingImage.setAttribute('aria-label',mappingDescription);
    mappingStudy.append(mappingSummary,mappingImage);mappingFrame.closest('.entry').after(mappingStudy);
    const svg=pages.generate.querySelector('.circuit');
    const print=document.createElement('figure');print.className='circuit-print';
    print.innerHTML='<div class="contributed-circuit" role="img" aria-label="Hand-drawn 64-room quantum circuit: six source, six destination and six property qubits, with rotations, entangling gates and measurements"></div><figcaption class="circuit-caption">Dan’s full job sketch: 64 rooms, 18 qubits. Six source and six destination qubits set leap odds; six property qubits set room character. A 16-room job uses 14 qubits.</figcaption>';
    svg.before(print);
    const details=document.createElement('details');details.className='circuit-study';details.innerHTML='<summary>Inspect the original drawing</summary><div class="contributed-circuit" role="img" aria-label="Larger view of the handwritten 64-room circuit"></div>';
    pages.generate.querySelector('.spread').append(details);
    const property=document.createElement('details');property.innerHTML='<summary>Property-register schematic</summary>';svg.replaceWith(property);property.append(svg);
  }
  return {apply,choices};
})();
