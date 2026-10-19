import re,json,sys,statistics as st
uart=open('uart_run.log',errors='replace').read().splitlines()
t0=None
def parse_kv(s): return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',s)}
http=[];res=[];hb=[]
for l in uart:
    m=re.match(r'\[\+\s*([\d.]+)s\] (.*)',l)
    if not m: continue
    t=float(m.group(1)); body=m.group(2)
    if 'hram_http:' in body:
        d=parse_kv(body); d['t']=t; d['phase']=re.search(r'phase=(\w+)',body).group(1); d['path']=re.search(r'path=(\w+)',body).group(1); http.append(d)
    elif 'point=hram_30s' in body and 'resources:' in body:
        d=parse_kv(body); d['t']=t; res.append(d)
    elif 'heartbeat' in body:
        hb.append(t)
gaps=[b-a for a,b in zip(hb,hb[1:])]
print('heartbeats',len(hb),'max_gap_s',round(max(gaps),3))
print('hram_http lines',len(http),'result!=0:',[h for h in http if h['result']!=0][:3])
print('httpd_stack_hwm min/max',min(h['httpd_stack_hwm'] for h in http),max(h['httpd_stack_hwm'] for h in http))
for p in ('shell','status','temperatures','alerts'):
    pre=[h for h in http if h['path']==p and h['phase']=='pre']; sent=[h for h in http if h['path']==p and h['phase']=='sent']
    if not sent: continue
    print(p,'n',len(sent),'free_sent_min',min(h['internal_free'] for h in sent),'free_pre_min',min(h['internal_free'] for h in pre),'largest_min',min(h['internal_largest'] for h in http if h['path']==p),'internal_min',min(h['internal_min'] for h in sent))
if res:
    print('hram_30s n',len(res),'free min/max',min(r['internal_8bit_free_bytes'] for r in res),max(r['internal_8bit_free_bytes'] for r in res),'min_free_ever',min(r['internal_8bit_minimum_free_bytes'] for r in res),'largest min',min(r['internal_8bit_largest_free_block_bytes'] for r in res),'main_hwm min',min(r['stack_hwm_bytes'] for r in res))
    for r in res[:3]+res[-6:]: print(' t=%7.1f free=%d min=%d largest=%d hwm=%d'%(r['t'],r['internal_8bit_free_bytes'],r['internal_8bit_minimum_free_bytes'],r['internal_8bit_largest_free_block_bytes'],r['stack_hwm_bytes']))
