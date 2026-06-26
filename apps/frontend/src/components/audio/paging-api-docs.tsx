/**
 * Paging API documentation card for the Audio → Paging tab.
 */
export function PagingApiDocs(): React.JSX.Element {
  return (
    <div className="rounded-lg border border-border bg-card">
      <div className="border-b border-border px-6 py-4">
        <h2 className="font-medium text-card-foreground">API Documentation</h2>
        <p className="mt-0.5 text-sm text-muted-foreground">
          Use these endpoints from Shortcuts, Home Assistant, or other automation tools.
        </p>
      </div>

      <div className="grid gap-6 p-6 text-sm text-foreground">
        <section className="grid gap-2">
          <h3 className="font-medium">Authentication</h3>
          <p className="text-muted-foreground">
            Set a paging API key in <strong>Audio → Settings → Paging API Key</strong>. Send it on
            automation requests using either header:
          </p>
          <pre className="overflow-x-auto rounded-md border border-border/60 bg-muted/40 p-3 text-xs">
            {`Authorization: Bearer YOUR_KEY
X-HomePi-Paging-Key: YOUR_KEY`}
          </pre>
        </section>

        <section className="grid gap-2">
          <h3 className="font-medium">Speak (TTS page)</h3>
          <p className="text-muted-foreground">
            Converts text to speech, activates whole-house paging, plays the announcement, then
            returns zones to their previous state.
          </p>
          <pre className="overflow-x-auto rounded-md border border-border/60 bg-muted/40 p-3 text-xs">
            {`POST /api/audio/paging/speak
Content-Type: application/json
Authorization: Bearer YOUR_KEY

{
  "text": "Dinner is ready.",
  "includeChime": true,
  "voiceId": "en_US-lessac-medium"
}`}
          </pre>
          <p className="text-xs text-muted-foreground">
            Optional fields: <code className="text-foreground">voiceId</code>,{" "}
            <code className="text-foreground">includeChime</code> (default false),{" "}
            <code className="text-foreground">onBusy</code> (<code>reject</code> or{" "}
            <code>queue</code>).
          </p>
        </section>

        <section className="grid gap-2">
          <h3 className="font-medium">Chime only</h3>
          <p className="text-muted-foreground">
            Plays the active chime through all zones without speech. Omit{" "}
            <code className="text-foreground">chimeId</code> to use the default chime.
          </p>
          <pre className="overflow-x-auto rounded-md border border-border/60 bg-muted/40 p-3 text-xs">
            {`POST /api/audio/paging/chime
Content-Type: application/json
Authorization: Bearer YOUR_KEY

{
  "chimeId": "default"
}`}
          </pre>
        </section>

        <section className="grid gap-2">
          <h3 className="font-medium">Example: iPhone Shortcut</h3>
          <ol className="list-decimal space-y-1 pl-5 text-muted-foreground">
            <li>Add action <strong>Get Contents of URL</strong>.</li>
            <li>
              URL: <code className="text-foreground">http://homepi.local/api/audio/paging/speak</code>{" "}
              (or your HomePi hostname).
            </li>
            <li>Method: <strong>POST</strong>.</li>
            <li>
              Headers: <code className="text-foreground">Authorization</code> ={" "}
              <code className="text-foreground">Bearer YOUR_KEY</code>,{" "}
              <code className="text-foreground">Content-Type</code> ={" "}
              <code className="text-foreground">application/json</code>.
            </li>
            <li>
              Request body: JSON with your message, for example{" "}
              <code className="text-foreground">{`{"text":"Someone is at the door","includeChime":true}`}</code>.
            </li>
          </ol>
        </section>

        <section className="grid gap-2">
          <h3 className="font-medium">UI-only endpoints</h3>
          <p className="text-xs text-muted-foreground">
            Preview and test actions in this tab use unauthenticated UI routes such as{" "}
            <code className="text-foreground">/api/audio/paging/chimes/preview</code> and{" "}
            <code className="text-foreground">/api/audio/paging/preview-page</code>. External
            automation should use <code className="text-foreground">/speak</code> and{" "}
            <code className="text-foreground">/chime</code> with your API key.
          </p>
        </section>
      </div>
    </div>
  );
}
