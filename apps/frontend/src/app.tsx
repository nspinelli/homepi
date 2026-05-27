/**
 * HomePi application shell — module UI is added later.
 */
export function App(): React.JSX.Element {
  return (
    <div className="app">
      <header className="app-header">
        <h1>HomePi</h1>
        <p className="subtitle">home automation platform</p>
      </header>
      <main className="app-main">
        <section className="card">
          <h2>Welcome</h2>
          <p>
            Application shell is running. Development assumes{" "}
            <code>https://homepi.local</code> via NGINX.
          </p>
        </section>
      </main>
    </div>
  );
}
