/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./app/**/*.{js,ts,jsx,tsx,mdx}",
    "./pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./components/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        radar: {
          dark: '#030d08',
          green: '#10b981',
          bright: '#34d399',
          faint: '#064e3b',
          grid: '#065f46',
          alert: '#ef4444',
          warning: '#f59e0b',
        }
      }
    },
  },
  plugins: [],
}
