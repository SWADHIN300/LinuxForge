import './globals.css';
import { ThemeProvider } from '@/lib/ThemeContext';
import { NotificationProvider } from '@/lib/NotificationContext';
import Sidebar from '@/components/Navbar';

export const metadata = {
  title: 'LinuxForge — Container Management Dashboard',
  description: 'A powerful Linux container simulator with real-time monitoring, log viewing, health checks, stack management, network topology, and more.',
};

export default function RootLayout({ children }) {
  return (
    <html lang="en" className="dark" suppressHydrationWarning>
      <head>
        <link rel="preconnect" href="https://fonts.googleapis.com" />
        <link rel="preconnect" href="https://fonts.gstatic.com" crossOrigin="anonymous" />
        <link
          href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@300;400;500;600;700&family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap"
          rel="stylesheet"
        />
        <link
          href="https://fonts.googleapis.com/css2?family=Material+Symbols+Outlined:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200"
          rel="stylesheet"
        />
      </head>
      <body className="antialiased overflow-x-hidden">
        <ThemeProvider>
          <NotificationProvider>
            {/* Sidebar + TopBar */}
            <Sidebar />
            {/* Main content offset for sidebar + topbar */}
            <div className="lf-main">
              <div className="lf-page">
                {children}
              </div>
            </div>
          </NotificationProvider>
        </ThemeProvider>
      </body>
    </html>
  );
}
