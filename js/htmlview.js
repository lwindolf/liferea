// vim: set ts=4 sw=4:
/*
 * @file htmlview.js  html view for node + item display
 *
 * Copyright (C) 2021-2025 Lars Windolf <lars.windolf@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

function setBase(uri) {
	var base = document.createElement("base");
	base.setAttribute("href", uri);
	document.head.appendChild(base);
}

// returns first occurence of a given metadata type
function metadata_get(obj, key) {
	var metadata = obj.metadata;

	if (!metadata)
		return null;

	var results = metadata.filter((e) => key in e);
	if (results.length == 0)
		return null;

	return results[0][key];
}

function parseStatus(parsePhase, errorCode) {

	if (errorCode == 0 || errorCode > parsePhase)
		return "✅";
	if (errorCode == parsePhase)
		return "⛔";
	if (parsePhase > errorCode)
		return "⬜";
}

function loadNode(data, baseURL = null, direction) {
	let node = JSON.parse(decodeURIComponent(data));
	let publisher	= metadata_get(node, "publisher");
	let author		= metadata_get(node, "author");
	let copyright	= metadata_get(node, "copyright");
	let description	= metadata_get(node, "description");
	let homepage	= metadata_get(node, "homepage");

	// FIXME
	console.log(node);

	if (node.title) {
		/* Set title for it to appear in e.g. desktop MPRIS playback controls */
		document.title = node.title;
	}
	if (baseURL)
			setBase(baseURL);

	document.body.innerHTML = `
	<div class="item" dir="${direction}">
		<header class="content">
			<h1>
				<a href="${homepage}">${node.title}</a>
			</h1>
			${node.source?`<div>Source: <a href="${node.source}">${node.source}</a></div>`:''}
			${author?`<div>Author: ${author}</div>`:''}
			${publisher?`<div>Publisher: ${publisher}</div>`:''}
			${copyright?`<div>Copyright: ${copyright}</div>`:''}
		</header>

		${node.error?`
		<div id="errors">
			<span>There was a problem when fetching this subscription!</span>
			<ul>	let item = JSON.parse(decodeURI(data));

				<li>
					${parseStatus(1, node.error)} <span>1. Authentication</span>
				</li>
				<li>
					${parseStatus(2, node.error)} <span>2. Download</span>
				</li>
				<li>
					${parseStatus(4, node.error)} <span>3. Feed Discovery</span>
				</li>
				<li>
					${parseStatus(8, node.error)} <span>4. Parsing</span>
				</li>
			</ul>

    		<span class="details">
      		<b><span>Details:</span></b>

				${(1 == node.error)?`
					<p><span>Authentication failed. Please check the credentials and try again!</span></p>
				`:''}
				${(2 == node.error)?`
					${node.httpError?`
						<p>
						${node.httpErrorCode >= 100?`HTTP ${node.httpErrorCode}: `:''}<br/>
						${node.httpError}
						</p>
					`:''}

					${node.updateError?`
						<p>
						<span>There was an error when downloading the feed source:</span>
						<pre class="errorOutput">${node.updateError}</pre>
						</p>
					`:''}

					${node.filterError?`
						<p>
						<span>There was an error when running the feed filter command:</span>
						<pre class="errorOutput">${node.filterError}</pre>
						</p>
					`:''}
				`:''}
				${(4 == node.error)?`
					<p><span>The source does not point directly to a feed or a webpage with a link to a feed!</span></p>
				`:''}
				${(8 == node.error)?`
					<p><span>Sorry, the feed could not be parsed!</span></p>
					<pre class="errorOutput">${node.parseError}</pre>
					<p><span>You may want to contact the author/webmaster of the feed about this!</span></p>
				`:''}
			</span>
		</div>
		`:''}

		<div id="content" class="content">
			<div id="description">${description || ''}</div>
		</div>
	</div>`;

    contentCleanup ();
}

function loadItem(data, baseURL = null, direction) {
	let item = JSON.parse(decodeURIComponent(data));
	let richContent = metadata_get(item, "richContent");
	let description = item.description;
	// FIXME: use this!
	/*let author		= metadata_get(item, "author");
	let creator		= metadata_get(item, "creator");
	let sharedby	= metadata_get(item, "sharedby");
	let via			= metadata_get(item, "via");
	let related		= metadata_get(item, "related");
	let point		= metadata_get(item, "point");
	let mediaviews	= metadata_get(item, "mediaviews");
	let ratingavg	= metadata_get(item, "mediastarRatingavg");
	let ratingmax	= metadata_get(item, "mediastarRatingMax");
	let gravatar	= metadata_get(item, "gravatar");*/
	let mediathumb	= metadata_get(item, "mediathumbnail");
	let mediadesc	= metadata_get(item, "mediadescription");
	let slash		= metadata_get(item, "slash");
	let youtube_embed;

	let videos = item.enclosures.filter((enclosure) => enclosure.mime?.startsWith('video/'));
	let audios = item.enclosures.filter((enclosure) => enclosure.mime?.startsWith('audio/'));

	if (richContent) {
		let shadowDoc = document.implementation.createHTMLDocument();
		shadowDoc.body.innerHTML = richContent;

		let article = new Readability(shadowDoc, {charThreshold: 100}).parse();
		if (article) {
			description = article.content;
		}
	}

	// FIXME
	console.log(item);

	if (item.title) {
		/* Set title for it to appear in e.g. desktop MPRIS playback controls */
		document.title = item.title;
	}
	if (baseURL)
			setBase(baseURL);

	document.body.innerHTML = `
	<div class="item" dir="${direction}">
		<header class="content">
			<div class="feedTitle">${item?.feedTitle}</div>
			<h1>
				<a href="${item.source}">${item.title}</a>
			</h1>
			${slash?`
			<div class="slash">
				<span class="slashSection">Section</span><span class="slashValue">${slash.split(',')[0]}</span>
  				<span class="slashDepartment">Department</span><span class="slashValue">${slash.split(',')[1]}</span>
			</div>
			`:''}
		</header>			

		<!-- cleaned by DOMPurify -->
		<div id="content" class="content">
			<div id="description">${description}</div></div>		
		</div>

		<!-- not cleaned by DOMPurify -->
		<div class="content">
			<!-- embed suitable enclosures -->
			${(audios.length > 0)?`
				<div id='enclosureAudio'>
					<audio class="enclosure" controls="controls" preload="none" src="${audios[0].url}"></audio>
					${(audios.length > 1)?`
					<select>
						${audios.map((enclosure) => `<option value="${enclosure.url}">${enclosure.url}</option>`).join('')}
					</select>
					`:''}
				</div>`:""}
			${(videos.length > 0)?`
				<div id='enclosureVideo'>
					<video class="enclosure" controls="controls" preload="none" src="${videos[0].url}"></video>
					${(videos.length > 1)?`
					<select>
						${videos.map((enclosure) => `<option value="${enclosure.url}">${enclosure.url}</option>`).join('')}
					</select>
					`:''}					
				</div>`:""}
		</div>
	</div>`;

    // If there are no images and we have a thumbnail, add it
    if (document.querySelectorAll('img').length == 0 && mediathumb) {
		let img = document.createElement('img');
		img.src = mediathumb;
		img.alt = mediadesc;
		document.getElementById('description').prepend(img);
    }

	// Convert all lazy-loaded images
	document.querySelectorAll('img[data-src]').forEach((img) => {
		img.src = img.getAttribute('data-src');
	});

    // Setup audio/video <select> handler
    document.querySelector('#enclosureVideo select')?.addEventListener("change", (e) => {
		document.querySelector('#enclosureVideo video').src = e.target.options[e.target.selectedIndex].value;
		document.querySelector('#enclosureVideo video').play();
    });
    document.getElementById('#enclosureAudio select')?.addEventListener("change", (e) => {
		document.querySelector('#enclosureAudio audio').src = e.target.options[e.target.selectedIndex].value;
		document.querySelector('#enclosureVideo audio').play();
    });

	let youtubeMatch = item.source.match(/https:\/\/www\.youtube\.com\/watch\?v=([\w-]+)/);
	if (youtubeMatch)
		youtube_embed (youtubeMatch[1]);

    contentCleanup ();

    return true;
}

/**
 * Different content cleanup tasks
 */
function contentCleanup() {

	// Run DOMPurify
	content = document.getElementById('content').innerHTML;
	document.getElementById('content').innerHTML = DOMPurify.sanitize(content);

	// Fix inline SVG sizes
	const svgMinWidth = 50;
	document.getElementById('content')
		.querySelectorAll('svg')
		.forEach((el) => {
			const h = el.getAttribute('height');
			const w = el.getAttribute('width');
			if(h && w) {
				if(w < svgMinWidth)
					el.parentNode.removeChild(el);
				return;
			}

			const viewbox = el.getAttribute('viewBox');
			if(!viewbox)
				return;

			const size = viewbox.split(/\s+/);
			if(size.length != 4)
				return;

			// Drop smaller SVGs that are usually just layout decorations
			if((size[2] - size[0]) < svgMinWidth) {
				el.parentNode.removeChild(el);
				return;
			}

			// Properly size larger SVGs
			el.width = size[2] - size[0];
			el.heigth = size[3] - size[1];
		});

	// Drop empty elements (to get rid of empty picture/video/iframe divs)
	const emptyRegex = new RegExp("^\s*$");
	document.getElementById('content')
		.querySelectorAll(":only-child")
		.forEach((el) => {
			if(el.innerHTML.length == 1)
				el.parentNode.removeChild(el);
		});
}

function youtube_embed(id) {
	var container = document.getElementById(id);
	container.innerHTML = '<iframe width="640" height="480" src="https://www.youtube.com/embed/' + id + '?autoplay=1" frameborder="0" allowfullscreen="1" allow="autoplay; allowfullscreen"></iframe>';

	return false;
}
