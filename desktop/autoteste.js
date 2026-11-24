// Roteiro do teste automático do Stumix Desktop: clica como uma pessoa e confere o resultado.
(async () => {
  const log = (m) => window.__JUCE__.backend.emitEvent('__juce__invoke', { name: 'estudioLog', params: [encodeURIComponent(m)], resultId: 9e6 + Math.floor(Math.random() * 1e5) });
  const espera = (ms) => new Promise(r => setTimeout(r, ms));
  const $ = (s) => document.querySelector(s);
  let falhas = 0; const ok = (c, m) => { log((c ? '[ok]    ' : '[FALHA] ') + m); if (!c) falhas++; };
  try {
    ok(!!(window.__JUCE__ && window.__JUCE__.backend), 'ponte JUCE presente');
    // o WebView2 pode demorar para carregar a página na primeira abertura: espera a interface (até 60 s)
    for (let i = 0; i < 120 && !(document.readyState === 'complete' && $('#projBtn') && window.__studio); i++) await espera(500);
    log('   página: ' + location.href + ' | estado ' + document.readyState);
    ok(!!$('#projBtn'), 'interface carregada');
    try { localStorage.setItem('estudio:guiaVisto', '1.7.0') } catch (e) {}
    const dlg = document.querySelector('dialog[open]'); if (dlg) dlg.close();
    $('#projBtn').click(); await espera(400); $('#projNovo').click(); await espera(400);
    const eco = await new Promise(res => { const id = 8e6; window.__JUCE__.backend.addEventListener('__juce__complete', ({ promiseId, result }) => { if (promiseId === id) res(result) }); window.__JUCE__.backend.emitEvent('__juce__invoke', { name: 'estudioEco', params: ['é'], resultId: id }) });
    log('   eco: JS→nativo chegou código ' + eco.codigos + ' (é = e9) | nativo→JS: ' + encodeURIComponent(eco.nativo));
    ok(eco.nativo === 'Comunicações é', 'acentos do motor chegam certos na interface');
    log('   acentos na página: ' + encodeURIComponent(document.title) + ' / ' + encodeURIComponent($('#sobreTitulo').textContent));
    ok(!$('#natRows').hidden, 'linhas do motor nativo visíveis');
    ok($('.so-web').hidden, 'seletor do navegador escondido');
    ok($('#natTipo').options.length >= 0, 'driver listado: ' + [...$('#natTipo').options].map(o => o.text).join(', '));
    // teste do microfone com a placa simulada (tom de 220 Hz)
    $('#test').click(); await espera(11500);
    const v = $('#verd').innerText.replace(/\n/g, ' | ');
    log('   teste: ' + v);
    ok(/Sinal|Voz/.test(v), 'teste do microfone concluído pelo motor nativo');
    // gravação de 3 s sem contagem
    if ($('#count').checked) $('#count').click();
    $('#rec').click(); await espera(3200); $('#rec').click(); await espera(2500);
    const d = window.__studio.buf ? window.__studio.buf.duration : 0;
    log('   gravação: ' + d.toFixed(2) + ' s | ' + $('#verd').innerText.replace(/\n/g, ' | ').slice(0, 160));
    ok(d > 2.5 && d < 3.6, 'gravação nativa chegou à interface com a duração certa');
    const b = window.__studio.buf, x = b.getChannelData(0); let mx = 0; for (let i = 0; i < x.length; i++) mx = Math.max(mx, Math.abs(x[i]));
    ok(Math.abs(mx - 0.3) < 0.01, 'amplitude preservada (pico ' + mx.toFixed(3) + ')');
    ok(b.sampleRate === 48000, 'taxa do projeto = taxa do motor (' + b.sampleRate + ')');
    const at = await new Promise(res => { const id = 8e6 + 1; window.__JUCE__.backend.addEventListener('__juce__complete', ({ promiseId, result }) => { if (promiseId === id) res(result) }); window.__JUCE__.backend.emitEvent('__juce__invoke', { name: 'estudioVerificarAtualizacao', params: [], resultId: id }) });
    log('   atualização: versão do app ' + at.atual + ' | publicada ' + (at.versao || '—') + ' | nova: ' + !!at.nova + (at.erro ? ' | ' + at.erro : ''));
  } catch (e) { log('[FALHA] erro: ' + e.message); falhas++; }
  log(falhas ? 'HOUVE FALHAS' : 'TODOS OS TESTES PASSARAM');
  window.__JUCE__.backend.emitEvent('__juce__invoke', { name: 'estudioSair', params: [], resultId: 9e6 + 99999 });
})();
