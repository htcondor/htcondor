function initLinks() {
    document.querySelectorAll('*[class*="link:"]').forEach(node => {
        node.classList.forEach(className => {
            if (className.startsWith('link:')) {
                console.log(node)
                const tool = className.split(':')[1];
                if ( ! node.dataset.linkAttached) {
                    node.addEventListener('click', () => {
                        window.open(`${tool}.html`, '_self');
                    });
                    node.dataset.linkAttached = "true";
                }
            }
        });
    });
}

window.addEventListener('load', () => {
    const POLL_INTERVAL = 100;
    const MAX_ATTEMPTS = 50;
    let lastCount = -1;
    let attempts = 0;

    const interval = setInterval(() => {
        const nodes = document.querySelectorAll('g.mindmap-node');

        if (nodes.length > 0 && nodes.length === lastCount) {
            clearInterval(interval);
            initLinks();
        }

        lastCount = nodes.length;

        // safety stop (5 seconds)
        if (attempts++ > MAX_ATTEMPTS) {
            console.warn("mermaid diagrams not detected in time");
            clearInterval(interval);
            initLinks();
        }
    }, POLL_INTERVAL);
});
