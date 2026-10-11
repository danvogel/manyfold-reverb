// Hosted entry: static Python imports and active top-level lifetime are required by Qollab.
import * as workflow from 'manyspace.workflow';
const notebook = await ManyspaceNotebook.mount({workflow,backend,sendFile});
await new Promise(() => {});
