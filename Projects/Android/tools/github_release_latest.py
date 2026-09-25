"""Mark a verified upload Latest and refresh the repository download panel.

Run after each release publisher has verified its asset digests. Existing README
content, historical releases, repository visibility and source code are preserved.
Git Credential Manager supplies credentials in memory; they are never logged.
"""
import argparse,base64,hashlib,json,subprocess,urllib.error,urllib.parse,urllib.request
from pathlib import Path

START='<!-- NOODOE_LATEST_DOWNLOADS_BEGIN -->'
END='<!-- NOODOE_LATEST_DOWNLOADS_END -->'

def client(owner):
    p=subprocess.run(['git','-c','credential.interactive=false','credential','fill'],
        input=f'protocol=https\nhost=github.com\nusername={owner}\n\n',capture_output=True,text=True,timeout=30)
    fields=dict(line.split('=',1) for line in p.stdout.splitlines() if '=' in line)
    if p.returncode or not fields.get('password'):raise SystemExit('GitHub authentication unavailable.')
    headers={'Authorization':'Bearer '+fields['password'],'Accept':'application/vnd.github+json',
             'X-GitHub-Api-Version':'2022-11-28','User-Agent':'Noodoe-Release-Publisher'}
    def api(path,method='GET',body=None,missing=False):
        request_headers=dict(headers);data=None
        if body is not None:data=json.dumps(body).encode();request_headers['Content-Type']='application/json'
        req=urllib.request.Request('https://api.github.com'+path,data=data,headers=request_headers,method=method)
        try:
            with urllib.request.urlopen(req,timeout=45) as response:return json.load(response)
        except urllib.error.HTTPError as error:
            if missing and error.code==404:return None
            raise SystemExit(f'GitHub HTTP {error.code}; inspect current state before retrying.')
    assert api('/user')['login'].lower()==owner.lower(),'Wrong GitHub account'
    return api

def promote(api,record,output,inspect=False):
    """Verify uploaded assets, promote explicitly, then atomically update README."""
    repository=record['repository'];base='/repos/'+repository
    repo=api(base)
    # Preserve the visibility observed by the publisher. This tool never changes
    # it; older records without the field still require a private repository.
    expected_private=record.get('private',True)
    assert isinstance(expected_private,bool) and repo['private']==expected_private,'Repository visibility changed since upload'
    assert repo.get('permissions',{}).get('push')
    tag=record['tag'];release=api(base+'/releases/tags/'+urllib.parse.quote(tag,safe=''))
    assert not release['draft'],'Finish artifact publication before selecting Latest'
    assets={a['name']:a for a in release['assets']}
    for expected in record['assets']:
        actual=assets[expected['name']]
        assert actual['state']=='uploaded' and actual['size']==expected['bytes']
        assert actual.get('digest')=='sha256:'+expected['sha256'],'Upload differs from verified artifact'
    apk=[a for a in assets.values() if a['name'].endswith('.apk')]
    bundle=[a for a in assets.values() if a['name'].endswith('.zip')]
    assert len(apk)==len(bundle)==1,'Select one verified APK and one installer ZIP'
    branch=repo['default_branch'];query='?ref='+urllib.parse.quote(branch,safe='')
    readme=api(base+'/readme'+query,missing=True)
    old=base64.b64decode(readme['content']).decode('utf-8') if readme else ''
    path=readme['path'] if readme else 'README.md'
    if inspect:
        print(json.dumps({'repository':repository,'tag':tag,'prerelease':release['prerelease'],
              'default_branch':branch,'readme_path':path,'readme':old},ensure_ascii=False,indent=2));return
    output.mkdir(parents=True,exist_ok=True)
    (output/'github-readme-before.md').write_text(old,encoding='utf-8')
    latest_url=f'https://github.com/{repository}/releases/latest'
    panel='\n'.join([START,'# 최신 다운로드','',f'**{release["name"]}**','',
        f'- **[Android APK 다운로드]({apk[0]["browser_download_url"]})**',
        f'- **[설치 ZIP 다운로드]({bundle[0]["browser_download_url"]})**',
        f'- [항상 최신 릴리스 보기]({latest_url})','',
        f'현재 버전: `{tag}`. 위 APK와 ZIP을 한 쌍으로 사용하세요.',
        'Bootstrap 수정이 포함된 업데이트는 해당 릴리스의 설치 순서를 확인하세요.',
        '검증 범위와 알려진 제한은 릴리스 설명에 기록되어 있습니다.',END])
    if START in old or END in old:
        assert old.count(START)==old.count(END)==1,'Ambiguous download panel'
        start=old.index(START);end=old.index(END)+len(END);assert start<end
        new=old[:start]+panel+old[end:]
    else:new=panel+'\n\n'+old
    release=api(base+f'/releases/{release["id"]}','PATCH',
                {'prerelease':False,'make_latest':'true'})
    assert api(base+'/releases/latest')['id']==release['id'],'Latest selection failed'
    if new!=old:
        body={'message':f'Show latest APK and installer: {tag}','branch':branch,
              'content':base64.b64encode(new.encode('utf-8')).decode()}
        if readme:body['sha']=readme['sha']
        api(base+'/contents/'+urllib.parse.quote(path,safe='/'),'PUT',body)
    checked=api(base+'/readme'+query)
    assert base64.b64decode(checked['content']).decode('utf-8')==new,'README readback differs'
    current=api(base+'/releases/latest');assert current['id']==release['id'] and not current['prerelease']
    result={'repository':repository,'tag':tag,'latest':latest_url,'release':current['html_url'],
            'readme':checked['html_url'],'apk':apk[0]['browser_download_url'],
            'installer':bundle[0]['browser_download_url'],'asset_sha256_verified':True,
            'readme_sha256':hashlib.sha256(new.encode()).hexdigest()}
    (output/'github-latest.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2));return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',type=Path,required=True)
    p.add_argument('--inspect',action='store_true');a=p.parse_args()
    record=json.loads(a.record.read_text(encoding='utf-8'))
    promote(client(record['repository'].split('/')[0]),record,a.record.parent,a.inspect)

if __name__=='__main__':main()
